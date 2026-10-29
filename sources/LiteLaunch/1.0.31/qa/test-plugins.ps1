$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
. "$source\plugins\manager.ps1" -LibraryOnly
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$work=Join-Path $workspace 'temp\LiteLaunch-build-1.0.31\plugin-qa'
New-Item -ItemType Directory -Path $work -Force|Out-Null
$PluginRoot=Join-Path $work ('installed-'+[guid]::NewGuid().ToString('N'))
$binary=Join-Path $workspace 'outputs\LiteLaunch\1.0.31\LiteLaunch-Plugin-CursorSwitcher-1.0.1-x64.exe'
$script:checks=0
function Check($value,[string]$name){if(-not $value){throw ('FAIL '+$name)};$script:checks++;'PASS '+$name}
function Reject([scriptblock]$action,[string]$name){$failed=$false;try{& $action|Out-Null}catch{$failed=$true};Check $failed $name}
$plugin=[pscustomobject]@{id='cursor-switcher';name='Cursor';version='1.0.1';minHostVersion='1.0.31';releaseTag='plugin-cursor-v1.0.1';asset='LiteLaunch-Plugin-CursorSwitcher-1.0.1-x64.exe';sha256=(Get-FileHash -LiteralPath $binary).Hash;productName='LiteLaunch Cursor Switcher'}
$catalog=[pscustomobject]@{schemaVersion=1;plugins=@($plugin)}
Check ((Read-PluginCatalog $catalog).plugins.Count -eq 1) 'independent plugin release tag accepted without changing host version'
$catalog.plugins=@($plugin,$plugin);Reject {Read-PluginCatalog $catalog} 'duplicate plugin ID rejected';$catalog.plugins=@($plugin)
$plugin.id='../escape';Reject {Read-PluginCatalog $catalog} 'catalog path traversal rejected';$plugin.id='cursor-switcher'
$plugin.releaseTag='../tag';Reject {Read-PluginCatalog $catalog} 'invalid plugin release tag rejected';$plugin.releaseTag='plugin-cursor-v1.0.1'
$plugin.sha256='';Reject {Read-PluginCatalog $catalog} 'missing plugin digest rejected';$plugin.sha256=(Get-FileHash -LiteralPath $binary).Hash
$plugin.asset='LiteLaunch.exe';Reject {Read-PluginCatalog $catalog} 'host binary cannot be treated as plugin';$plugin.asset='LiteLaunch-Plugin-CursorSwitcher-1.0.1-x64.exe'
Reject {Assert-OwnedPluginPath (Join-Path $work 'outside.exe')} 'plugin writes constrained to owned plugin directory'
Assert-PluginBinary $binary $plugin;Check $true 'plugin digest and PE product version verified'
$plugin.productName='Wrong product';Reject {Assert-PluginBinary $binary $plugin} 'wrong plugin product rejected';$plugin.productName='LiteLaunch Cursor Switcher'
Install-PluginBinary $binary $plugin
$installed=Get-InstalledPlugin $plugin;Check ($installed.Record.version -eq '1.0.1') 'optional local install and installed record read back'
Check ($installed.Record.minHostVersion -eq '1.0.31') 'host compatibility persisted for later independent updates'
$release=[pscustomobject]@{tag_name='plugin-cursor-v1.0.1';draft=$false;prerelease=$false;assets=@([pscustomobject]@{name=$plugin.asset;state='uploaded';digest=('sha256:'+$plugin.sha256);browser_download_url=('https://github.com/'+$Repository+'/releases/download/plugin-cursor-v1.0.1/'+$plugin.asset)})}
Check ((Get-PluginReleaseAsset $release $plugin.asset).name -eq $plugin.asset) 'plugin assets resolved from their own stable release'
$release.prerelease=$true;Reject {Get-PluginReleaseAsset $release $plugin.asset} 'prerelease rejected';$release.prerelease=$false
$release.assets[0].browser_download_url='https://example.com/plugin.exe';Reject {Get-PluginReleaseAsset $release $plugin.asset} 'foreign download rejected'
$bytes=[IO.File]::ReadAllBytes($installed.Path);$bytes[300]=$bytes[300] -bxor 1;[IO.File]::WriteAllBytes($installed.Path,$bytes)
Reject {Get-InstalledPlugin $plugin} 'tampered installed plugin cannot launch'
Check ((Get-InstalledPlugin $plugin -MetadataOnly).Record.version -eq '1.0.1') 'damaged plugin metadata remains available for removal or repair'
Install-PluginBinary $binary $plugin;Check ((Get-InstalledPlugin $plugin).Record.version -eq '1.0.1') 'verified local package repairs a corrupted installation atomically'
$tokens=$null;$errors=$null;[void][Management.Automation.Language.Parser]::ParseFile("$source\plugins\manager.ps1",[ref]$tokens,[ref]$errors);Check ($errors.Count -eq 0) 'plugin manager parses cleanly'
$localFolder=Join-Path $work ('packages-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $localFolder|Out-Null
Check ($null -eq (Find-LocalPluginPackage $plugin @($localFolder))) 'missing optional package returns an explicit install path'
$package=Join-Path $localFolder $plugin.asset;Copy-Item -LiteralPath $binary -Destination $package
Check ((Find-LocalPluginPackage $plugin @($localFolder)) -eq $package) 'first click discovers the exact verified local package'
$bad=Join-Path $work ('bad-packages-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $bad|Out-Null;[IO.File]::WriteAllText((Join-Path $bad $plugin.asset),'invalid package')
Check ($null -eq (Find-LocalPluginPackage $plugin @($bad))) 'first click never installs a wrong or damaged adjacent executable'
Check ((Find-LocalPluginPackage $plugin @($bad,$localFolder)) -eq $package) 'invalid adjacent package cannot hide a later verified package'
'RESULT checks='+$script:checks+' failures=0; no installed user plugin or cursor was changed'
