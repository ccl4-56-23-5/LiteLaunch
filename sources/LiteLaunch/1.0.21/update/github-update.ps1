param([string]$ExecutablePath,[string]$CurrentVersion='1.0.21',[int]$OwnerPID=0,[switch]$LibraryOnly)
$ErrorActionPreference='Stop'
$Repository='ccl4-56-23-5/LiteLaunch'
function Get-StableVersion([string]$Tag) {
    if($Tag -notmatch '^v?(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?$'){throw '发布标签不是受支持的稳定版本号。'}
    $text=$Tag -replace '^v','';if(($text.Split('.')).Count -eq 3){$text+='.0'};return [version]$text
}
function Get-ReleaseAsset($Release,[string]$Repo) {
    if($Release.draft -or $Release.prerelease){throw '不接受草稿或预发布版本。'}
    $version=Get-StableVersion $Release.tag_name
    $assets=@($Release.assets|Where-Object {$_.name -ceq 'LiteLaunch.exe' -and $_.state -eq 'uploaded'})
    if($assets.Count -ne 1){throw '发布中缺少唯一的LiteLaunch.exe。'}
    $asset=$assets[0]
    $expected='https://github.com/'+$Repo+'/releases/download/'+$Release.tag_name+'/LiteLaunch.exe'
    if($asset.browser_download_url -cne $expected){throw '更新文件地址与指定仓库及标签不匹配。'}
    if($asset.digest -notmatch '^sha256:([a-fA-F0-9]{64})$'){throw '发布缺少GitHub生成的SHA-256摘要，已停止更新。'}
    $hash=$Matches[1]
    if([long]$asset.size -lt 65536 -or [long]$asset.size -gt 67108864){throw '更新文件大小异常。'}
    return [pscustomobject]@{Version=$version;Url=$asset.browser_download_url;SHA256=$hash;Size=[long]$asset.size}
}
function Assert-Download([string]$Path,$Asset) {
    $file=Get-Item -LiteralPath $Path
    if($file.Length -ne $Asset.Size -or (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $Asset.SHA256){throw '下载文件大小或SHA-256校验失败。'}
    $version=Get-StableVersion $file.VersionInfo.FileVersion
    if($version -ne $Asset.Version -or $file.VersionInfo.ProductName -ne 'LiteLaunch'){throw 'EXE产品名称或版本与发布不匹配。'}
}
function Install-Package([string]$Package,[string]$Target,[string]$ExpectedHash) {
    if((Get-FileHash -LiteralPath $Package).Hash -ine $ExpectedHash){throw '安装前更新文件再次校验失败。'}
    $backup=$Target+'.update-backup';if(Test-Path -LiteralPath $backup){throw '发现上次更新备份，请先检查'+$backup}
    Move-Item -LiteralPath $Target -Destination $backup
    try {
        Move-Item -LiteralPath $Package -Destination $Target
        if((Get-FileHash -LiteralPath $Target).Hash -ine $ExpectedHash){throw '安装文件校验失败。'}
    }catch{
        if(Test-Path -LiteralPath $Target){Move-Item -LiteralPath $Target -Destination $Package -Force}
        Move-Item -LiteralPath $backup -Destination $Target
        throw
    }
    return $backup
}
if($LibraryOnly){return}
Add-Type -AssemblyName System.Windows.Forms
$mutex=New-Object Threading.Mutex($false,'Local\LiteLaunch.Update.Check')
$locked=$false;$backup=$null;$completed=$false
try {
    $locked=$mutex.WaitOne(0);if(-not $locked){return}
    $ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath)
    if((Get-Item -LiteralPath $ExecutablePath).Attributes -band [IO.FileAttributes]::ReparsePoint){throw '不支持通过链接路径替换程序。'}
    $owner=Get-Process -Id $OwnerPID -ErrorAction Stop
    if($owner.Path -ine $ExecutablePath){throw '当前程序身份校验失败。'}
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    $progress=New-Object Windows.Forms.Form
    $progress.Text='LiteLaunch · 检查GitHub更新';$progress.Width=440;$progress.Height=120;$progress.StartPosition='CenterScreen'
    $label=New-Object Windows.Forms.Label;$label.Text='正在检查'+$Repository+'的正式版本…';$label.AutoSize=$true;$label.Left=20;$label.Top=25;$progress.Controls.Add($label);$progress.Show();$progress.Refresh()
    try {$release=Invoke-RestMethod -Uri ('https://api.github.com/repos/'+$Repository+'/releases/latest') -Headers @{'User-Agent'='LiteLaunch-Updater';Accept='application/vnd.github+json';'X-GitHub-Api-Version'='2022-11-28'} -TimeoutSec 25}
    catch {throw '无法取得GitHub正式版本。仓库尚未发布、网络不可达或请求受限时均不会修改程序。'+$_.Exception.Message}
    $asset=Get-ReleaseAsset $release $Repository
    $progress.Hide()
    if($asset.Version -le (Get-StableVersion $CurrentVersion)){[Windows.Forms.MessageBox]::Show('当前已是最新正式版本。','LiteLaunch更新')|Out-Null;return}
    $answer=[Windows.Forms.MessageBox]::Show(('当前版本：'+$CurrentVersion+"`n最新版本："+$asset.Version+"`n`n是否下载更新？"),'LiteLaunch更新','YesNo','Question')
    if($answer -ne 'Yes'){return}
    $download=Join-Path $PSScriptRoot 'LiteLaunch.download.exe'
    $label.Text='正在下载并校验更新文件…';$progress.Show();$progress.Refresh()
    Invoke-WebRequest -UseBasicParsing -Uri $asset.Url -OutFile $download -TimeoutSec 60 -Headers @{'User-Agent'='LiteLaunch-Updater'}
    Assert-Download $download $asset
    $progress.Hide()
    $answer=[Windows.Forms.MessageBox]::Show("下载校验完成。关闭LiteLaunch并安装新版本？`n应用列表、EVER设置和文件均保留。请先完成正在编辑的重命名。",'LiteLaunch更新','YesNo','Question')
    if($answer -ne 'Yes'){return}
    $sibling=Join-Path ([IO.Path]::GetDirectoryName($ExecutablePath)) ('LiteLaunch.update-'+[Guid]::NewGuid().ToString('N')+'.exe')
    if(Test-Path -LiteralPath ($ExecutablePath+'.update-backup')){throw '发现上次更新备份，请先检查。'}
    Copy-Item -LiteralPath $download -Destination $sibling
    $quit=Start-Process -FilePath $ExecutablePath -ArgumentList '--quit' -PassThru -WindowStyle Hidden
    if(-not $quit.WaitForExit(10000) -or $quit.ExitCode -ne 0){throw '无法请求程序退出。'}
    if(-not $owner.WaitForExit(30000)){throw '程序尚未退出，未替换任何文件。'}
    try {$backup=Install-Package $sibling $ExecutablePath $asset.SHA256}
    catch {if(Test-Path -LiteralPath $ExecutablePath){Start-Process -FilePath $ExecutablePath -ArgumentList '--hidden' -WindowStyle Hidden};throw}
    $new=Start-Process -FilePath $ExecutablePath -ArgumentList '--hidden' -PassThru -WindowStyle Hidden
    $startupLog=Join-Path $env:LOCALAPPDATA 'LiteLaunch\startup.log';$ready=$false
    for($i=0;$i -lt 30;$i++){Start-Sleep -Milliseconds 250;$new.Refresh();if($new.HasExited){break};if(Test-Path -LiteralPath $startupLog){$tail=(Get-Content -LiteralPath $startupLog -Tail 40)-join "`n";if($tail -match ("pid="+$new.Id+" tray registered=1")){$ready=$true;break}}}
    if(-not $ready){
        if(-not $new.HasExited){& $ExecutablePath --quit;[void]$new.WaitForExit(10000)}
        if(-not $new.HasExited){throw '新版本未完成启动且仍在运行；保留更新备份，请手工检查。'}
        $failed=Join-Path $PSScriptRoot 'LiteLaunch.failed.exe';Move-Item -LiteralPath $ExecutablePath -Destination $failed
        Move-Item -LiteralPath $backup -Destination $ExecutablePath;$backup=$null
        Start-Process -FilePath $ExecutablePath -ArgumentList '--hidden' -WindowStyle Hidden
        throw '新版本启动失败，已恢复旧版。'
    }
    $completed=$true
    Add-Type -AssemblyName Microsoft.VisualBasic
    [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($backup,'OnlyErrorDialogs','SendToRecycleBin');$backup=$null
    [Windows.Forms.MessageBox]::Show('已更新至'+$asset.Version+'。','LiteLaunch更新')|Out-Null
}catch{
    $message=$_.Exception.Message
    if($backup -and -not $completed -and (Test-Path -LiteralPath $backup)){
        try {
            $canRestore=$true
            if($new){$new.Refresh();if(-not $new.HasExited){& $ExecutablePath --quit;$canRestore=$new.WaitForExit(10000)}}
            if(-not $canRestore){throw '新进程尚未退出'}
            if(Test-Path -LiteralPath $ExecutablePath){Move-Item -LiteralPath $ExecutablePath -Destination (Join-Path $PSScriptRoot ('failed-'+[Guid]::NewGuid().ToString('N')+'.exe'))}
            Move-Item -LiteralPath $backup -Destination $ExecutablePath;$backup=$null
            Start-Process -FilePath $ExecutablePath -ArgumentList '--hidden' -WindowStyle Hidden
            $message+="`n已恢复旧版。"
        }catch{$message+="`n自动恢复未完成，旧EXE保留在："+$backup}
    }
    [Windows.Forms.MessageBox]::Show($message,'LiteLaunch更新未完成','OK','Warning')|Out-Null
}
finally{
    if($progress){$progress.Dispose()}
    if($sibling -and (Test-Path -LiteralPath $sibling)){Remove-Item -LiteralPath $sibling -ErrorAction SilentlyContinue}
    if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()
    # Only this script's unique extracted directory is eligible for cleanup.
    if($completed){Remove-Item -LiteralPath (Join-Path $PSScriptRoot 'LiteLaunch.download.exe') -ErrorAction SilentlyContinue}
}