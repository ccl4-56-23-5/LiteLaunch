param([string]$CatalogPath,[string]$HostVersion='1.0.31',[string]$HostExecutable,[string]$RequestedPlugin,[string]$ActionFile,[string]$SourceWindow='0',[int]$Slot=-1,[switch]$LibraryOnly,[switch]$UpdateOnly)
$ErrorActionPreference='Stop'
$Repository='ccl4-56-23-5/LiteLaunch'
$PluginRoot=Join-Path $env:LOCALAPPDATA 'LiteLaunch\Plugins'
function Get-PluginFileHash([string]$Path){
 $stream=[IO.File]::OpenRead($Path);$algorithm=[Security.Cryptography.SHA256]::Create()
 try{return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-','')}finally{$algorithm.Dispose();$stream.Dispose()}
}
function Write-PluginLaunchLog([string]$Event,[string]$Detail=''){
if($LibraryOnly){return}
 try{('{0:o} {1} {2}' -f [DateTime]::Now,$Event,$Detail)|Add-Content -LiteralPath (Join-Path $PSScriptRoot 'launch.log') -Encoding utf8}catch{}
}
trap {
 Write-PluginLaunchLog 'failed' ($_.Exception.Message+' '+$_.ScriptStackTrace)
 try{Add-Type -AssemblyName System.Windows.Forms;[Windows.Forms.MessageBox]::Show(('插件未能启动：'+$_.Exception.Message+"`r`n诊断记录："+(Join-Path $PSScriptRoot 'launch.log')),'LiteLaunch插件')|Out-Null}catch{}
 break
}
function Read-PluginCatalog($Value){
 if($Value.schemaVersion -ne 1 -or @($Value.plugins).Count -gt 40){throw '插件目录格式不受支持。'}
 $seen=@{}
 foreach($p in $Value.plugins){
  if($p.id -notmatch '^[a-z][a-z0-9-]{0,47}$' -or $seen.ContainsKey($p.id)){throw '插件ID重复或无效。'};$seen[$p.id]=$true
  if($p.version -notmatch '^\d+\.\d+\.\d+$' -or $p.minHostVersion -notmatch '^\d+\.\d+\.\d+$'){throw '插件版本无效。'}
  if($p.releaseTag -and $p.releaseTag -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$'){throw '插件发布标签无效。'}
  if($p.asset -cnotmatch ('^LiteLaunch-Plugin-[A-Za-z0-9]+-'+[regex]::Escape($p.version)+'-x64\.exe$') -or $p.sha256 -notmatch '^[a-fA-F0-9]{64}$'){throw '插件文件或摘要无效。'}
  if([string]::IsNullOrWhiteSpace($p.name) -or $p.name.Length -gt 50 -or [string]::IsNullOrWhiteSpace($p.productName)){throw '插件名称缺失。'}
 }
 return $Value
}
function Merge-PluginCatalog($Builtin,$Cached){
 $items=@();$used=@{}
 foreach($entry in $Builtin.plugins){$new=@($Cached.plugins|Where-Object id -ceq $entry.id);if($new.Count -eq 1 -and [version]$new[0].version -ge [version]$entry.version){$items+=$new[0]}else{$items+=$entry};$used[$entry.id]=$true}
 foreach($entry in $Cached.plugins){if(-not $used.ContainsKey($entry.id)){$items+=$entry;$used[$entry.id]=$true}}
 Read-PluginCatalog ([pscustomobject]@{schemaVersion=1;plugins=$items})
}
function Assert-OwnedPluginPath([string]$Path){
 $base=[IO.Path]::GetFullPath($PluginRoot).TrimEnd('\');$full=[IO.Path]::GetFullPath($Path)
 if(-not $full.StartsWith($base+'\',[StringComparison]::OrdinalIgnoreCase)){throw '插件路径超出组件目录。'}
 $walk=$full;while($walk.Length -ge $base.Length){if(Test-Path -LiteralPath $walk){if((Get-Item -LiteralPath $walk -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw '插件目录不能是重解析点。'}};$walk=[IO.Path]::GetDirectoryName($walk);if(-not $walk){break}}
 return $full
}
function Get-InstalledPlugin($Plugin,[switch]$MetadataOnly){
 $record=Assert-OwnedPluginPath (Join-Path $PluginRoot ($Plugin.id+'\installed.json'))
 if(-not (Test-Path -LiteralPath $record)){return $null}
 $value=Get-Content -LiteralPath $record -Raw -Encoding UTF8|ConvertFrom-Json
  if($value.id -cne $Plugin.id -or $value.version -notmatch '^\d+\.\d+\.\d+$' -or $value.minHostVersion -notmatch '^\d+\.\d+\.\d+$' -or $value.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or $value.productName -cne $Plugin.productName){throw '本地插件登记信息无效。'}
 $path=Assert-OwnedPluginPath (Join-Path $PluginRoot ($Plugin.id+'\'+$value.version+'\plugin.exe'))
 if(-not $MetadataOnly){Assert-PluginBinary $path $value}
 return [pscustomobject]@{Record=$value;Path=$path}
}
function Assert-PluginBinary([string]$Path,$Plugin){
 $file=Get-Item -LiteralPath $Path
 if($file.Length -lt 4096 -or $file.Length -gt 67108864 -or (Get-PluginFileHash $Path) -ine $Plugin.sha256){throw '插件文件大小或SHA-256校验失败。'}
 if($file.VersionInfo.ProductName.Trim() -cne $Plugin.productName -or ([version]$file.VersionInfo.FileVersion) -ne ([version]($Plugin.version+'.0'))){throw '插件产品或版本与目录不匹配。'}
}
function Install-PluginBinary([string]$Path,$Plugin){
 Assert-PluginBinary $Path $Plugin
 $folder=Assert-OwnedPluginPath (Join-Path $PluginRoot ($Plugin.id+'\'+$Plugin.version))
 New-Item -ItemType Directory -Path $folder -Force|Out-Null
 $dest=Join-Path $folder 'plugin.exe'
 $needsCopy=$true;if(Test-Path -LiteralPath $dest){try{Assert-PluginBinary $dest $Plugin;$needsCopy=$false}catch{}}
 if($needsCopy){
  $temporary=Join-Path $folder ('download-'+[guid]::NewGuid().ToString('N')+'.tmp')
  $replaced=Join-Path $folder ('replaced-'+[guid]::NewGuid().ToString('N')+'.tmp')
  try{Copy-Item -LiteralPath $Path -Destination $temporary;Assert-PluginBinary $temporary $Plugin;if(Test-Path -LiteralPath $dest){[IO.File]::Replace($temporary,$dest,$replaced)}else{Move-Item -LiteralPath $temporary -Destination $dest}}
  finally{if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary};if(Test-Path -LiteralPath $replaced){Remove-Item -LiteralPath $replaced -ErrorAction SilentlyContinue}}
 }
 $record=Join-Path (Split-Path $folder -Parent) 'installed.json';$temporary=$record+'.tmp'
 @{id=$Plugin.id;name=$Plugin.name;version=$Plugin.version;minHostVersion=$Plugin.minHostVersion;sha256=$Plugin.sha256;productName=$Plugin.productName}|ConvertTo-Json|Set-Content -LiteralPath $temporary -Encoding utf8
 if(Test-Path -LiteralPath $record){[IO.File]::Replace($temporary,$record,[NullString]::Value)}else{[IO.File]::Move($temporary,$record)}
}
function Get-PluginReleaseAsset($Release,[string]$Name){
 if($Release.draft -or $Release.prerelease -or $Release.tag_name -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$'){throw '插件发布不是有效稳定版本。'}
 $matches=@($Release.assets|Where-Object {$_.name -ceq $Name -and $_.state -eq 'uploaded'})
 if($matches.Count -ne 1){throw ('此发布缺少插件文件：'+$Name)}
 $asset=$matches[0]
 if($asset.browser_download_url -cne ('https://github.com/'+$Repository+'/releases/download/'+$Release.tag_name+'/'+$Name) -or $asset.digest -notmatch '^sha256:[A-Fa-f0-9]{64}$'){throw '插件下载地址或GitHub摘要无效。'}
 return $asset
}
function Get-PluginCatalogReleases($Releases){
 # GitHub may put its marked-latest host release first, ahead of newer plugin releases.
 # Choose by publication time; neither API array order nor /releases/latest is a plugin channel.
 @($Releases|Where-Object {-not $_.draft -and -not $_.prerelease -and $_.published_at -and @($_.assets|Where-Object name -ceq 'LiteLaunch-plugins.json').Count}|Sort-Object @{Expression={[DateTimeOffset]$_.published_at};Descending=$true})
}
function Fetch-PluginCatalog {
 $headers=@{'User-Agent'='LiteLaunch-Plugins';Accept='application/vnd.github+json'}
 $releases=Invoke-RestMethod -Uri ('https://api.github.com/repos/'+$Repository+'/releases?per_page=30') -Headers $headers -TimeoutSec 30
 foreach($release in (Get-PluginCatalogReleases $releases)){
  if($release.draft -or $release.prerelease -or -not @($release.assets|Where-Object name -ceq 'LiteLaunch-plugins.json').Count){continue}
  $asset=Get-PluginReleaseAsset $release 'LiteLaunch-plugins.json'
  if($asset.size -lt 50 -or $asset.size -gt 262144){throw '插件目录大小异常。'}
  $file=Join-Path $PSScriptRoot 'remote-catalog.json';Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $file -Headers $headers -TimeoutSec 40
  if((Get-Item -LiteralPath $file).Length -ne $asset.size -or ('sha256:'+(Get-PluginFileHash $file)) -ine $asset.digest){throw '插件目录摘要校验失败。'}
  $catalog=Read-PluginCatalog (Get-Content -LiteralPath $file -Raw -Encoding UTF8|ConvertFrom-Json)
  $cache=Assert-OwnedPluginPath (Join-Path $PluginRoot 'catalog-cache.json');New-Item -ItemType Directory -Path $PluginRoot -Force|Out-Null
  $catalog|ConvertTo-Json -Depth 10|Set-Content -LiteralPath $cache -Encoding utf8
  return [pscustomobject]@{Catalog=$catalog;Release=$release}
 }
 throw 'GitHub正式发布尚未提供插件目录。测试阶段可使用“本地安装”。'
}
function Launch-InstalledPlugin($Plugin,[string]$Action=''){
 $installed=Resolve-PluginForLaunch $Plugin;if(-not $installed){throw '请先下载或本地安装此插件。'}
 if([version]$installed.Record.version -lt [version]$Plugin.version){throw ('请先把'+$Plugin.name+'更新到'+$Plugin.version+'。可使用同目录的新版插件EXE进行本地安装。')}
 if([version]$installed.Record.minHostVersion -gt [version]$HostVersion){throw '已安装插件需要更高版本的LiteLaunch。'}
 $arguments=''
 if($Plugin.id -in @('batch-renamer','ever-rename')){
  if($HostExecutable){$env:LITELAUNCH_HOST_EXE=$HostExecutable}
  $window=0L;if(-not [long]::TryParse($SourceWindow,[ref]$window) -or $window -lt 0){throw '来源窗口无效。'}
  $arguments='--source-window '+$window
  if($Action){$file=Join-Path $PSScriptRoot ('ever-action-'+[guid]::NewGuid().ToString('N')+'.txt');[IO.File]::WriteAllText($file,$Action,[Text.Encoding]::Unicode);$arguments+=' --action-file "'+$file+'"'}
 }
 if($arguments){$process=Start-Process -FilePath $installed.Path -ArgumentList $arguments -WorkingDirectory (Split-Path $installed.Path -Parent) -WindowStyle Normal -PassThru}else{$process=Start-Process -FilePath $installed.Path -WorkingDirectory (Split-Path $installed.Path -Parent) -WindowStyle Normal -PassThru}
 Write-PluginLaunchLog 'plugin-started' ($Plugin.id+' pid='+$process.Id)
 return $process
}
function Find-LocalPluginPackage($Plugin,[string[]]$Directories){
 foreach($directory in $Directories){
  if([string]::IsNullOrWhiteSpace($directory)){continue}
  $path=Join-Path $directory $Plugin.asset
  if(Test-Path -LiteralPath $path -PathType Leaf){try{Assert-PluginBinary $path $Plugin;return $path}catch{Write-PluginLaunchLog 'local-package-rejected' $_.Exception.Message}}
 }
 return $null
}
function Get-LocalPluginDirectories {
 $directories=@($PSScriptRoot)
 if($HostExecutable){$directories+=Split-Path $HostExecutable -Parent}
 try{$saved=(Get-ItemProperty -LiteralPath 'HKCU:\Software\LiteLaunch' -Name PackageSource -ErrorAction Stop).PackageSource;if($saved){$directories+=$saved}}catch{}
 return $directories
}
function Resolve-PluginForLaunch($Plugin,[string[]]$Directories=@()){
 $installed=Get-InstalledPlugin $Plugin
 if(( -not $installed -or [version]$installed.Record.version -lt [version]$Plugin.version) -and [version]$Plugin.minHostVersion -le [version]$HostVersion){
  if($Directories.Count -eq 0){$Directories=Get-LocalPluginDirectories}
  $package=Find-LocalPluginPackage $Plugin $Directories
  if($package){Install-PluginBinary $package $Plugin;$installed=Get-InstalledPlugin $Plugin;Write-PluginLaunchLog 'local-package-installed' ($Plugin.id+' version='+$Plugin.version)}
 }
 return $installed
}
function Test-PluginProcessActive([string]$Path){
 foreach($process in [Diagnostics.Process]::GetProcessesByName('plugin')){
  try{if($process.MainModule.FileName -ieq $Path){return $true}}catch{}finally{$process.Dispose()}
 }
 return $false
}
function Save-PluginDownload($Asset,[string]$Path){
 Invoke-WebRequest -UseBasicParsing -Uri $Asset.browser_download_url -OutFile $Path -TimeoutSec 60 -Headers @{'User-Agent'='LiteLaunch-Plugins'}
}
function Get-PluginUpdateInfo([string]$Id){
 $online=Fetch-PluginCatalog;$matches=@($online.Catalog.plugins|Where-Object id -ceq $Id)
 if($matches.Count -ne 1){throw 'GitHub目录中未找到此插件。'}
 $plugin=$matches[0];$installed=Get-InstalledPlugin $plugin -MetadataOnly
 $current=if($installed){$installed.Record.version}else{'未安装'}
 $valid=$false;if($installed){try{$null=Get-InstalledPlugin $plugin;$valid=$true}catch{}}
 $state=if($installed -and [version]$installed.Record.version -gt [version]$plugin.version){'LocalNewer'}elseif($valid -and $installed.Record.version -eq $plugin.version -and $installed.Record.sha256 -ieq $plugin.sha256){'UpToDate'}else{'Available'}
 [pscustomobject]@{State=$state;Plugin=$plugin;Catalog=$online.Catalog;Release=$online.Release;CurrentVersion=$current;LatestVersion=$plugin.version}
}
function Update-PluginFromGitHub([string]$Id,$Info=$null){
 if($Id -notmatch '^[a-z][a-z0-9-]{0,47}$'){throw '插件ID无效。'}
 $mutex=New-Object Threading.Mutex($false,('Local\LiteLaunch.PluginUpdate.'+$Id));$locked=$false
 try{
  try{$locked=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$locked=$true}
  if(-not $locked){throw '此插件已在检查更新，请稍候。'}
  $online=if($Info){$Info}else{Fetch-PluginCatalog}
  $matches=@($online.Catalog.plugins|Where-Object id -ceq $Id)
  if($matches.Count -ne 1){throw 'GitHub目录中未找到此插件。'}
  $plugin=$matches[0]
  if([version]$plugin.minHostVersion -gt [version]$HostVersion){throw ('此插件需要LiteLaunch '+$plugin.minHostVersion+'或更高版本，请先更新基座。')}
  $installed=Get-InstalledPlugin $plugin -MetadataOnly
  if($installed -and [version]$installed.Record.version -gt [version]$plugin.version){return [pscustomobject]@{State='LocalNewer';Plugin=$plugin;Catalog=$online.Catalog;Message='本地插件版本高于GitHub版本，已保留本地版本。'}}
  $valid=$false
  if($installed){try{$null=Get-InstalledPlugin $plugin;$valid=$true}catch{}}
  if($valid -and $installed.Record.version -eq $plugin.version -and $installed.Record.sha256 -ieq $plugin.sha256){return [pscustomobject]@{State='UpToDate';Plugin=$plugin;Catalog=$online.Catalog;Message=($plugin.name+' '+$plugin.version+' 已是最新版本。')}}
  $running=$installed -and (Test-PluginProcessActive $installed.Path)
  if($running -and $installed.Record.version -eq $plugin.version){throw '此插件正在运行，请先关闭插件窗口后再修复更新。'}
  $release=$online.Release
  if($plugin.releaseTag -and $plugin.releaseTag -cne $release.tag_name){$release=Invoke-RestMethod -Uri ('https://api.github.com/repos/'+$Repository+'/releases/tags/'+[uri]::EscapeDataString($plugin.releaseTag)) -Headers @{'User-Agent'='LiteLaunch-Plugins'} -TimeoutSec 30}
  $asset=Get-PluginReleaseAsset $release $plugin.asset
  if($asset.digest -ine ('sha256:'+$plugin.sha256) -or $asset.size -lt 4096 -or $asset.size -gt 67108864){throw '插件附件与目录的大小或SHA-256不一致。'}
  $work=Assert-OwnedPluginPath (Join-Path $PluginRoot ($Id+'\download-'+[guid]::NewGuid().ToString('N')+'.tmp'))
  New-Item -ItemType Directory -Force -Path (Split-Path $work -Parent)|Out-Null
  try{
   Save-PluginDownload $asset $work
   if((Get-Item -LiteralPath $work).Length -ne $asset.size){throw '插件下载不完整。'}
   Assert-PluginBinary $work $plugin
   Install-PluginBinary $work $plugin
   $activated=Get-InstalledPlugin $plugin
   if($activated.Record.version -ne $plugin.version -or $activated.Record.sha256 -ine $plugin.sha256){throw '插件更新后核验失败。'}
  }finally{if(Test-Path -LiteralPath $work){Remove-Item -LiteralPath $work}}
  $message=$plugin.name+' 已更新到 '+$plugin.version+'，无需重启基座。'
  if($running){$message+=' 当前插件窗口继续保留，关闭并重新打开插件后使用新版。'}
  return [pscustomobject]@{State='Updated';Plugin=$plugin;Catalog=$online.Catalog;Message=$message;PreviousWindowPreserved=[bool]$running}
 }finally{if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()}
}
function Show-PluginUpdateWindow([string]$Id){
 $sharedUI=Join-Path $PSScriptRoot 'ui.ps1';if(-not (Test-Path -LiteralPath $sharedUI)){$sharedUI=Join-Path $PSScriptRoot '..\update\ui.ps1'};. $sharedUI
 $view=New-LiteLaunchUpdateView '更新插件' '插件独立更新，无需更新基座'
 $descriptor=@($catalog.plugins|Where-Object id -ceq $Id);if($descriptor.Count -eq 1){$view.Heading.Text=$descriptor[0].name}
 $view.Form.Add_Shown({
  try{$view.Plan=Get-PluginUpdateInfo $Id;$plan=$view.Plan;$view.Heading.Text=$plan.Plugin.name
   $available=$plan.State -eq 'Available';$status=if($available){'发现新版本，可直接更新此插件。'}elseif($plan.State -eq 'LocalNewer'){'本地版本较新。'}else{'已是最新版本。'}
   $action=if($available){'更新到 '+$plan.LatestVersion}else{'已是最新版本'}
   if([version]$plan.Plugin.minHostVersion -gt [version]$HostVersion){$status='此版本需要基座 '+$plan.Plugin.minHostVersion+' 或更高版本。';$available=$false;$action='需要更新基座'}
   Set-LiteLaunchUpdateState $view $plan.CurrentVersion $plan.LatestVersion $status $action $available
  }catch{Set-LiteLaunchUpdateState $view $view.Current.Text '未获取' $_.Exception.Message '检查未完成' $false}
 }.GetNewClosure())
 $view.Primary.Add_Click({
  $view.Primary.Enabled=$false;$view.Close.Enabled=$false;$view.Progress.Visible=$true;$view.Status.Text='正在更新到 '+$view.Plan.LatestVersion+'…';$view.Form.Refresh()
  try{$view.Result=Update-PluginFromGitHub $Id $view.Plan;Set-LiteLaunchUpdateState $view $view.Result.Plugin.version $view.Plan.LatestVersion $view.Result.Message '更新完成' $false}
  catch{Set-LiteLaunchUpdateState $view $view.Plan.CurrentVersion $view.Plan.LatestVersion $_.Exception.Message ('重试更新到 '+$view.Plan.LatestVersion) $true}
  finally{$view.Close.Enabled=$true}
 }.GetNewClosure())
 try{[void]$view.Form.ShowDialog();return $view.Result}finally{$view.Form.Dispose()}
}
if($LibraryOnly){return}
Write-PluginLaunchLog 'requested' ('plugin='+$RequestedPlugin+' slot='+$Slot+' host='+$HostVersion)
$sharedUI=Join-Path $PSScriptRoot 'ui.ps1';if(-not (Test-Path -LiteralPath $sharedUI)){$sharedUI=Join-Path $PSScriptRoot '..\update\ui.ps1'};. $sharedUI
Initialize-LiteLaunchUI
Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class LiteLaunchPluginIni {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 public static extern bool WritePrivateProfileString(string section,string key,string value,string path);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern System.IntPtr FindWindow(string className,string name);
 [DllImport("user32.dll")] public static extern bool ShowWindow(System.IntPtr window,int command);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(System.IntPtr window);
}
'@
function Assign-PluginSlot([int]$Index,$Plugin){
 if($Index -lt 0 -or $Index -gt 3){throw '插件槽必须为1—4。'}
 $path=Join-Path $env:LOCALAPPDATA 'LiteLaunch\plugins.ini'
 if(-not (Test-Path -LiteralPath $path)){[IO.File]::WriteAllText($path,"; LiteLaunch plugin slots`r`n",[Text.Encoding]::Unicode)}
 $id=if($Plugin){$Plugin.id}else{''};$name=if($Plugin){$Plugin.name}else{''}
 if(-not [LiteLaunchPluginIni]::WritePrivateProfileString('Slots',('Slot'+$Index),$id,$path) -or -not [LiteLaunchPluginIni]::WritePrivateProfileString('Labels',('Slot'+$Index),$name,$path)){throw '保存插件槽失败。'}
}
[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
$builtin=Read-PluginCatalog (Get-Content -LiteralPath $CatalogPath -Raw -Encoding UTF8|ConvertFrom-Json);$catalog=$builtin
$cache=Assert-OwnedPluginPath (Join-Path $PluginRoot 'catalog-cache.json')
if(Test-Path -LiteralPath $cache){try{$catalog=Merge-PluginCatalog $builtin (Read-PluginCatalog (Get-Content -LiteralPath $cache -Raw -Encoding UTF8|ConvertFrom-Json))}catch{}}
$action=if($ActionFile){Get-Content -LiteralPath $ActionFile -Raw -Encoding Unicode}else{''}
if($UpdateOnly){Show-PluginUpdateWindow $RequestedPlugin|Out-Null;return}
if($RequestedPlugin){
 $requested=@($catalog.plugins|Where-Object id -ceq $RequestedPlugin)
 if($requested.Count -eq 1){try{
  $installed=Resolve-PluginForLaunch $requested[0]
  if($installed -and [version]$installed.Record.version -ge [version]$requested[0].version){Launch-InstalledPlugin $requested[0] $action|Out-Null;return}
 }catch{Write-PluginLaunchLog 'launch-error' $_.Exception.Message;[Windows.Forms.MessageBox]::Show($_.Exception.Message,'LiteLaunch插件')|Out-Null}}
}
$uiKey=if($RequestedPlugin -and $requested.Count -eq 1){$RequestedPlugin}else{'all'}
$uiTitle=if($uiKey -eq 'all'){'LiteLaunch · 插件'}else{'LiteLaunch · '+$requested[0].name}
$uiMutex=New-Object Threading.Mutex($false,('Local\LiteLaunch.PluginManager.UI.'+$uiKey));$uiLocked=$false
try{$uiLocked=$uiMutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$uiLocked=$true}
if(-not $uiLocked){$window=[LiteLaunchPluginIni]::FindWindow($null,$uiTitle);if($window -ne [IntPtr]::Zero){[void][LiteLaunchPluginIni]::ShowWindow($window,9);[void][LiteLaunchPluginIni]::SetForegroundWindow($window)};Write-PluginLaunchLog 'manager-focused' $uiKey;$uiMutex.Dispose();return}
$form=New-Object Windows.Forms.Form;Set-LiteLaunchFormStyle $form;$form.Text=$uiTitle;$form.Size=New-Object Drawing.Size(780,480);$form.MinimumSize=$form.Size;$form.StartPosition='CenterScreen';$form.Font=New-Object Drawing.Font('Microsoft YaHei UI',10)
$heading=New-Object Windows.Forms.Label;$heading.Text='按需安装 · 插件独立更新';$heading.SetBounds(20,16,720,28);$form.Controls.Add($heading)
$list=New-Object Windows.Forms.ListView;$list.View='Details';$list.FullRowSelect=$true;$list.MultiSelect=$false;$list.HideSelection=$false;$list.SetBounds(20,50,720,225)
$images=New-Object Windows.Forms.ImageList;$images.ImageSize=New-Object Drawing.Size(24,24);$images.ColorDepth='Depth32Bit'
foreach($entry in @(@('batch-renamer','renamer.ico'),@('cursor-switcher','cursor.ico'))){$file=Join-Path $PSScriptRoot $entry[1];if(Test-Path -LiteralPath $file){$icon=New-Object Drawing.Icon($file);try{$images.Images.Add($entry[0],$icon.ToBitmap())}finally{$icon.Dispose()}}};$list.SmallImageList=$images
@(@('插件',210),@('最新版本',100),@('本地版本',100),@('说明',300))|ForEach-Object {[void]$list.Columns.Add($_[0],$_[1])};$form.Controls.Add($list)
$status=New-Object Windows.Forms.Label;$status.Text='首次使用可选下载，不随主程序安装。';$status.SetBounds(20,286,720,42);$form.Controls.Add($status)
if($RequestedPlugin){$status.Text='此插件需要安装或更新。选择“下载 / 更新”或“本地安装”，完成后将直接启动。'}
$buttons=@{};$names=@('刷新目录','下载 / 更新','启动','本地安装','移除插件')
for($i=0;$i -lt $names.Count;$i++){$b=New-LiteLaunchButton $names[$i] ($i -eq 1);$b.SetBounds((20+$i*145),336,135,32);$form.Controls.Add($b);$buttons[$names[$i]]=$b}
$slots=New-Object Windows.Forms.ComboBox;$slots.DropDownStyle='DropDownList';1..4|ForEach-Object {[void]$slots.Items.Add('第'+$_+'个插件槽')};$slots.SelectedIndex=if($Slot -ge 0 -and $Slot -lt 4){$Slot}else{0};$slots.SetBounds(20,388,180,28);$form.Controls.Add($slots)
$pin=New-LiteLaunchButton '放入此槽';$pin.SetBounds(214,386,120,30);$form.Controls.Add($pin)
$clear=New-LiteLaunchButton '清空此槽';$clear.SetBounds(346,386,120,30);$form.Controls.Add($clear)
function Selected-Plugin {if($list.SelectedItems.Count -ne 1){throw '请先选择一个插件。'};return $list.SelectedItems[0].Tag}
function Refresh-PluginList {
 $list.Items.Clear()
 foreach($p in $script:catalog.plugins){
  $item=New-Object Windows.Forms.ListViewItem($p.name);$item.Tag=$p;if($images.Images.ContainsKey($p.id)){$item.ImageKey=$p.id};[void]$item.SubItems.Add($p.version)
  try{$installed=Get-InstalledPlugin $p;$local=if($installed){$installed.Record.version}else{'未安装'}}catch{$local='校验失败'}
  [void]$item.SubItems.Add($local);[void]$item.SubItems.Add($p.description);[void]$list.Items.Add($item)
  if($p.id -eq $RequestedPlugin){$item.Selected=$true}
 }
}
function Invoke-PluginAction([scriptblock]$Action){
 try{foreach($b in $buttons.Values){$b.Enabled=$false};$status.Text='正在处理…';$form.Refresh();& $Action;Refresh-PluginList}
 catch{$status.Text=$_.Exception.Message;[Windows.Forms.MessageBox]::Show($form,$_.Exception.Message,'插件操作未完成','OK','Warning')|Out-Null}
 finally{foreach($b in $buttons.Values){$b.Enabled=$true}}
}
$buttons['刷新目录'].Add_Click({Invoke-PluginAction {$online=Fetch-PluginCatalog;$script:catalog=Merge-PluginCatalog $builtin $online.Catalog;$status.Text='插件目录已刷新；更新插件无需更新主程序。'}})
$buttons['下载 / 更新'].Add_Click({Invoke-PluginAction {
 $p=Selected-Plugin;$result=Show-PluginUpdateWindow $p.id;if(-not $result){return};$script:catalog=Merge-PluginCatalog $builtin $result.Catalog
 if($result.State -ne 'LocalNewer'){Launch-InstalledPlugin $result.Plugin $action|Out-Null};$status.Text=$result.Message
}})
$buttons['启动'].Add_Click({Invoke-PluginAction {$p=Selected-Plugin;Launch-InstalledPlugin $p $action|Out-Null;$status.Text='已启动：'+$p.name}})
$list.Add_DoubleClick({$buttons['启动'].PerformClick()})
$buttons['本地安装'].Add_Click({Invoke-PluginAction {
 $p=Selected-Plugin;if([version]$p.minHostVersion -gt [version]$HostVersion){throw '主程序版本过低。'}
 $dialog=New-Object Windows.Forms.OpenFileDialog;$dialog.Filter='LiteLaunch插件 (*.exe)|*.exe';$dialog.Title='选择'+$p.asset
 $dialog.FileName=$p.asset
 try{if($dialog.ShowDialog($form) -eq 'OK'){Install-PluginBinary $dialog.FileName $p;Launch-InstalledPlugin $p $action|Out-Null;$status.Text='本地插件已安装并启动。'}}finally{$dialog.Dispose()}
}})
$buttons['移除插件'].Add_Click({Invoke-PluginAction {
 $p=Selected-Plugin;$installed=Get-InstalledPlugin $p -MetadataOnly;if(-not $installed){throw '此插件尚未安装。'}
 if([Windows.Forms.MessageBox]::Show($form,'移除插件程序？已有重命名历史、指针包和恢复记录会保留。','移除插件','YesNo','Question') -ne 'Yes'){return}
 $folder=Assert-OwnedPluginPath (Split-Path $installed.Path -Parent)
 if(@(Get-ChildItem -LiteralPath $folder -Recurse -Force|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw '插件目录包含重解析点。'}
 Remove-Item -LiteralPath $folder -Recurse;Remove-Item -LiteralPath (Join-Path (Split-Path $folder -Parent) 'installed.json');$status.Text='插件程序已移除，用户数据保留。'
}})
$pin.Add_Click({Invoke-PluginAction {Assign-PluginSlot $slots.SelectedIndex (Selected-Plugin);$status.Text='插件槽已保存，回到面板即可看到。'}})
$clear.Add_Click({Invoke-PluginAction {Assign-PluginSlot $slots.SelectedIndex $null;$status.Text='插件槽已清空。'}})
Refresh-PluginList
$form.Add_Shown({[void][LiteLaunchPluginIni]::ShowWindow($form.Handle,9);[void][LiteLaunchPluginIni]::SetForegroundWindow($form.Handle);if($RequestedPlugin){foreach($item in $list.Items){if($item.Tag.id -eq $RequestedPlugin){$item.Selected=$true;$item.Focused=$true;$item.EnsureVisible()}}};Write-PluginLaunchLog 'manager-visible' $uiKey})
try{[void]$form.ShowDialog()}finally{$form.Dispose();$images.Dispose();if($uiLocked){$uiMutex.ReleaseMutex()};$uiMutex.Dispose()}
