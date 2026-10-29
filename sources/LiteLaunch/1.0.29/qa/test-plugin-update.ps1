param([switch]$Online)
$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$work=Join-Path $workspace ('temp\LiteLaunch-build-1.0.29\plugin-update-qa-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $work|Out-Null
Copy-Item -LiteralPath "$source\plugins\manager.ps1" -Destination "$work\manager.ps1"
. "$work\manager.ps1" -LibraryOnly -HostVersion '1.0.29'
$realFetch=(Get-Command Fetch-PluginCatalog).ScriptBlock;$realSave=(Get-Command Save-PluginDownload).ScriptBlock
$script:checks=0;$script:downloads=0;$script:downloadMode='copy';$script:active=$false
function Check($value,[string]$name){if(-not $value){throw ('FAIL '+$name)};$script:checks++;'PASS '+$name}
function Reject([scriptblock]$action,[string]$name){$failed=$false;try{&$action|Out-Null}catch{$failed=$true};Check $failed $name}
$binary=Join-Path $workspace 'outputs\LiteLaunch\1.0.29\LiteLaunch-Plugin-CursorSwitcher-1.0.1-x64.exe'
$p=Get-Content -LiteralPath "$source\plugins\catalog.json" -Raw|ConvertFrom-Json
$script:latest=($p.plugins|Where-Object id -eq 'cursor-switcher')|ConvertTo-Json|ConvertFrom-Json
$script:latest.releaseTag='fixture-v1.0.1'
$oldCode=@'
using System.Reflection;
[assembly:AssemblyProduct("LiteLaunch Cursor Switcher")]
[assembly:AssemblyVersion("1.0.0.0")]
[assembly:AssemblyFileVersion("1.0.0.0")]
public static class Fixture{public static void Main(){} public const string Padding="PADDING";}
'@
$oldCode=$oldCode.Replace('PADDING',('x'*10000));[IO.File]::WriteAllText("$work\old.cs",$oldCode,[Text.UTF8Encoding]::new($true))
& 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe' /nologo /target:winexe /platform:x64 "/out:$work\old.exe" "$work\old.cs"
if($LASTEXITCODE -ne 0){throw 'Old-version fixture compile failed'}
$old=$script:latest|ConvertTo-Json|ConvertFrom-Json;$old.version='1.0.0';$old.asset='LiteLaunch-Plugin-CursorSwitcher-1.0.0-x64.exe';$old.sha256=(Get-FileHash -LiteralPath "$work\old.exe").Hash
function Online-Fixture {
 $plugin=$script:latest|ConvertTo-Json|ConvertFrom-Json
 $asset=[pscustomobject]@{name=$plugin.asset;state='uploaded';size=(Get-Item -LiteralPath $binary).Length;digest=('sha256:'+$plugin.sha256);browser_download_url=('https://github.com/'+$Repository+'/releases/download/'+$plugin.releaseTag+'/'+$plugin.asset)}
 return [pscustomobject]@{Catalog=[pscustomobject]@{schemaVersion=1;plugins=@($plugin)};Release=[pscustomobject]@{tag_name=$plugin.releaseTag;draft=$false;prerelease=$false;assets=@($asset)}}
}
function Fetch-PluginCatalog{return $script:mockOnline}
function Test-PluginProcessActive([string]$Path){return $script:active}
function Save-PluginDownload($Asset,[string]$Path){
 $script:downloads++
 if($script:downloadMode -eq 'network-error'){throw 'Simulated interrupted network'}
 Copy-Item -LiteralPath $binary -Destination $Path
 if($script:downloadMode -eq 'tamper'){$bytes=[IO.File]::ReadAllBytes($Path);$bytes[300]=$bytes[300] -bxor 1;[IO.File]::WriteAllBytes($Path,$bytes)}
 if($script:downloadMode -eq 'truncate'){[IO.File]::WriteAllBytes($Path,[byte[]](1,2,3))}
}
function Launch-InstalledPlugin {throw 'An update-only operation must never launch the plugin'}
function Fresh-Install {
 $script:PluginRoot=Join-Path $work ('plugins-'+[guid]::NewGuid().ToString('N'));Install-PluginBinary "$work\old.exe" $old
 $script:mockOnline=Online-Fixture;$script:active=$false;$script:downloadMode='copy'
}
function Old-Intact {
 $installed=Get-InstalledPlugin $old
 return $installed.Record.version -eq '1.0.0' -and (Get-FileHash -LiteralPath $installed.Path).Hash -eq $old.sha256
}
Fresh-Install
$result=Update-PluginFromGitHub 'cursor-switcher'
Check ($result.State -eq 'Updated' -and (Get-InstalledPlugin $script:latest).Record.version -eq '1.0.1') 'GitHub update activates a separately versioned plugin without launch'
Check ((Get-FileHash -LiteralPath (Join-Path $PluginRoot 'cursor-switcher\1.0.0\plugin.exe')).Hash -eq $old.sha256) 'old executable remains intact during side-by-side activation'
$before=$script:downloads;$result=Update-PluginFromGitHub 'cursor-switcher'
Check ($result.State -eq 'UpToDate' -and $script:downloads -eq $before) 'already-current valid plugin skips download'
foreach($mode in @('tamper','truncate','network-error')){
 Fresh-Install;$script:downloadMode=$mode;Reject {Update-PluginFromGitHub 'cursor-switcher'} ($mode+' download cannot activate');Check (Old-Intact) ($mode+' leaves previous plugin and registration usable')
}
Fresh-Install;$script:mockOnline.Catalog.plugins[0].minHostVersion='9.0.0'
Reject {Update-PluginFromGitHub 'cursor-switcher'} 'incompatible host version blocks plugin update';Check (Old-Intact) 'compatibility rejection preserves active plugin'
Fresh-Install;$script:mockOnline.Release.assets[0].browser_download_url='https://example.com/plugin.exe'
Reject {Update-PluginFromGitHub 'cursor-switcher'} 'foreign download endpoint is rejected';Check (Old-Intact) 'foreign endpoint cannot change registration'
Fresh-Install;$script:mockOnline.Release.assets[0].digest='sha256:'+('0'*64)
Reject {Update-PluginFromGitHub 'cursor-switcher'} 'catalog and release digest disagreement is rejected'
Fresh-Install;$script:mockOnline.Catalog.plugins=@();Reject {Update-PluginFromGitHub 'cursor-switcher'} 'removed plugin returns a clear failure'
Fresh-Install;$script:mockOnline.Catalog.plugins=@($script:mockOnline.Catalog.plugins[0],$script:mockOnline.Catalog.plugins[0]);Reject {Update-PluginFromGitHub 'cursor-switcher'} 'ambiguous plugin identity cannot update'
Reject {Update-PluginFromGitHub '../escape'} 'update ID cannot escape managed directory'
Fresh-Install;$script:mockOnline.Catalog.plugins[0].version='0.9.9';$before=$script:downloads;$result=Update-PluginFromGitHub 'cursor-switcher'
Check ($result.State -eq 'LocalNewer' -and (Old-Intact) -and $script:downloads -eq $before) 'remote older than local cannot downgrade'
Fresh-Install;$script:active=$true;$result=Update-PluginFromGitHub 'cursor-switcher'
Check ($result.State -eq 'Updated' -and $result.PreviousWindowPreserved) 'running older version continues while new version is registered'
Check ((Get-FileHash -LiteralPath (Join-Path $PluginRoot 'cursor-switcher\1.0.0\plugin.exe')).Hash -eq $old.sha256) 'running-version update does not replace old executable'
$installed=Get-InstalledPlugin $script:latest;$bytes=[IO.File]::ReadAllBytes($installed.Path);$bytes[300]=$bytes[300] -bxor 1;[IO.File]::WriteAllBytes($installed.Path,$bytes)
Reject {Update-PluginFromGitHub 'cursor-switcher'} 'in-use same-version repair asks for plugin closure without forcing exit'
$script:active=$false;$result=Update-PluginFromGitHub 'cursor-switcher'
Check ($result.State -eq 'Updated' -and (Get-FileHash -LiteralPath (Get-InstalledPlugin $script:latest).Path).Hash -eq $script:latest.sha256) 'closed damaged same-version plugin is repaired using validated bytes'
$tokens=$null;$errors=$null;[void][Management.Automation.Language.Parser]::ParseFile("$source\plugins\manager.ps1",[ref]$tokens,[ref]$errors);Check ($errors.Count -eq 0) 'Windows PowerShell manager parses cleanly'
if($Online){
 Set-Item Function:Fetch-PluginCatalog $realFetch;Set-Item Function:Save-PluginDownload $realSave
 Fresh-Install
 $hostPayload=Join-Path $workspace 'temp\LiteLaunch-installer\payload-1.0.29\LiteLaunch.exe';$hostBefore=(Get-FileHash -LiteralPath $hostPayload).Hash
 $result=Update-PluginFromGitHub 'cursor-switcher'
 Check ($result.State -eq 'Updated' -and (Get-InstalledPlugin $result.Plugin).Record.version -eq '1.0.1') 'actual GitHub release download installs verified cursor plugin in isolated profile'
 Check ((Get-FileHash -LiteralPath $hostPayload).Hash -eq $hostBefore) 'real online plugin update leaves host executable unchanged'
}
'RESULT checks='+$script:checks+' failures=0; real user plugin state was not accessed'
