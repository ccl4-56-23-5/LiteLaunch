param([string]$ExecutablePath,[string]$CurrentVersion='1.0.30',[int]$OwnerPID=0,[switch]$LibraryOnly)
$ErrorActionPreference='Stop'
function Get-OfficialRepository { return 'ccl4-56-23-5/LiteLaunch' }
function Get-OfficialReleaseEndpoint { return 'https://api.github.com/repos/'+(Get-OfficialRepository)+'/releases/latest' }
function Get-StableVersion([string]$Tag){
 $Tag=$Tag.Trim()
 if($Tag -notmatch '^v?(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?$'){throw '发布标签必须是稳定版本号。'}
 $text=$Tag -replace '^v','';if($text.Split('.').Count -eq 3){$text+='.0'};return [version]$text
}
function Resolve-Repository([string]$Address){
 $value=$Address.Trim().TrimEnd('/')
 if($value -match '^https://github\.com/([A-Za-z0-9-]+)/([A-Za-z0-9_.-]+)$'){$value=$Matches[1]+'/'+$Matches[2]}
 elseif($value -notmatch '^[A-Za-z0-9-]+/[A-Za-z0-9_.-]+$'){throw '请填写GitHub项目地址，如https://github.com/账号/仓库。'}
 if($value.EndsWith('.git')){$value=$value.Substring(0,$value.Length-4)}
 if($value -ine (Get-OfficialRepository)){throw '更新仅使用LiteLaunch官方GitHub仓库。'}
 return Get-OfficialRepository
}
function Get-ReleaseAsset($Release,[string]$Repo){
 $Repo=Resolve-Repository $Repo
 if($Release.draft -or $Release.prerelease){throw '不接受草稿或预发布版本。'}
 $version=Get-StableVersion $Release.tag_name
 $name='LiteLaunch-Setup-'+($Release.tag_name -replace '^v','')+'-x64.exe'
 $assets=@($Release.assets|Where-Object {$_.name -ceq $name -and $_.state -eq 'uploaded'})
 if($assets.Count -ne 1){throw ('此版本没有可用的唯一安装包：'+$name+"。`n请打开 https://github.com/"+$Repo+'/releases/latest 检查发布附件。主程序更新使用Setup安装包。')}
 $asset=$assets[0];$expected='https://github.com/'+$Repo+'/releases/download/'+$Release.tag_name+'/'+$name
 if($asset.browser_download_url -cne $expected){throw '安装包地址与指定仓库及版本不匹配。'}
 if($asset.digest -notmatch '^sha256:([a-fA-F0-9]{64})$'){throw '发布缺少GitHub生成的SHA-256摘要。'}
 $hash=$Matches[1];if([long]$asset.size -lt 65536 -or [long]$asset.size -gt 134217728){throw '安装包大小异常。'}
 [pscustomobject]@{Name=$name;Version=$version;Url=$asset.browser_download_url;SHA256=$hash;Size=[long]$asset.size}
}
function Get-UpdatePlan($Release,[string]$Repo,[string]$InstalledVersion){
 $Repo=Resolve-Repository $Repo
 if($Release.draft -or $Release.prerelease){throw '不接受草稿或预发布版本。'}
 $latest=Get-StableVersion $Release.tag_name
 $current=Get-StableVersion $InstalledVersion
 # No package is required for a release that will not be installed.
 if($latest -le $current){return [pscustomobject]@{UpdateAvailable=$false;Version=$latest;Asset=$null}}
 [pscustomobject]@{UpdateAvailable=$true;Version=$latest;Asset=(Get-ReleaseAsset $Release $Repo)}
}
function Get-UpdateFileHash([string]$Path){
 $stream=[IO.File]::OpenRead($Path);$algorithm=[Security.Cryptography.SHA256]::Create()
 try{return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','')}finally{$stream.Dispose();$algorithm.Dispose()}
}
function Assert-Download([string]$Path,$Asset){
 $file=Get-Item -LiteralPath $Path
 if($file.Length -ne $Asset.Size -or (Get-UpdateFileHash $Path) -ine $Asset.SHA256){throw '安装包大小或SHA-256校验失败。'}
 if((Get-StableVersion $file.VersionInfo.FileVersion) -ne $Asset.Version -or $file.VersionInfo.ProductName.Trim() -ne 'LiteLaunch' -or $file.VersionInfo.FileDescription.Trim() -notlike '*Setup*'){throw '安装包产品、版本或类型与发布不匹配。'}
}
function Get-OwnedStartupState([string]$TargetExe){
 $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
 $task=Get-ScheduledTask -TaskName ('LiteLaunch-AutoStart-'+$sid) -ErrorAction SilentlyContinue
 if($task){if($task.Author -ne 'LiteLaunch' -or @($task.Actions).Count -ne 1 -or $task.Actions[0].Execute -ine $TargetExe){throw '启动注册不属于当前安装。'};return [bool]$task.Settings.Enabled}
 return $false
}
function Get-SetupArguments([string]$TargetExe,[bool]$Startup){
 $folder=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($TargetExe))
 if($folder.Contains('"') -or $folder -eq [IO.Path]::GetPathRoot($folder)){throw '不支持的安装路径。'}
 # Normal visible wizard; no silent switches, process termination or direct EXE replacement.
 $task=if($Startup){'autostart'}else{'!autostart'}
 return '/NORESTART /DIR="'+$folder+'" /MERGETASKS="'+$task+'"'
}
function Show-UpdateNotice([string]$Message){
 $notice=New-Object Windows.Forms.Form;$notice.Text='LiteLaunch更新';$notice.Width=440;$notice.Height=120;$notice.StartPosition='CenterScreen'
 $text=New-Object Windows.Forms.Label;$text.Text=$Message;$text.AutoSize=$true;$text.Left=18;$text.Top=25;$notice.Controls.Add($text)
 $timer=New-Object Windows.Forms.Timer;$timer.Interval=1800
 $timer.Add_Tick({$timer.Stop();$notice.Close()}.GetNewClosure())
 try{$timer.Start();[void]$notice.ShowDialog()}finally{$timer.Dispose();$notice.Dispose()}
}
if($LibraryOnly){return}
Add-Type -AssemblyName System.Windows.Forms
$mutex=New-Object Threading.Mutex($false,'Local\LiteLaunch.Update.Check');$locked=$false;$progress=$null
try{
 $locked=$mutex.WaitOne(0);if(-not $locked){return}
 $ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath);$owner=Get-Process -Id $OwnerPID -ErrorAction Stop
 if([IO.Path]::GetFullPath($owner.Path) -ine $ExecutablePath){throw '当前程序身份校验失败。'}
 $repo=Get-OfficialRepository
 [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
 $progress=New-Object Windows.Forms.Form;$progress.Text='LiteLaunch · 检查安装版更新';$progress.Width=470;$progress.Height=120;$progress.StartPosition='CenterScreen'
 $label=New-Object Windows.Forms.Label;$label.Text='正在检查'+$repo+'的正式版本…';$label.AutoSize=$true;$label.Left=15;$label.Top=25;$progress.Controls.Add($label);$progress.Show();$progress.Refresh()
 try{$release=Invoke-RestMethod -Uri (Get-OfficialReleaseEndpoint) -Headers @{'User-Agent'='LiteLaunch-Updater';Accept='application/vnd.github+json'} -TimeoutSec 25}catch{throw '无法取得GitHub正式版本；没有修改程序。'+$_.Exception.Message}
 $plan=Get-UpdatePlan $release $repo $CurrentVersion
 if(-not $plan.UpdateAvailable){$progress.Hide();Show-UpdateNotice ('当前已是最新版本。当前版本：'+$CurrentVersion);return}
 $asset=$plan.Asset;$progress.Hide()
 $download=Join-Path $PSScriptRoot $asset.Name;$label.Text='正在下载安装包并校验…';$progress.Show();$progress.Refresh()
 Invoke-WebRequest -UseBasicParsing -Uri $asset.Url -OutFile $download -TimeoutSec 90 -Headers @{'User-Agent'='LiteLaunch-Updater'}
 Assert-Download $download $asset;$progress.Hide()
 Start-Process -FilePath $download -ArgumentList (Get-SetupArguments $ExecutablePath (Get-OwnedStartupState $ExecutablePath))
}catch{[Windows.Forms.MessageBox]::Show($_.Exception.Message,'LiteLaunch更新未完成','OK','Warning')|Out-Null}
finally{if($progress){$progress.Dispose()};if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()}
