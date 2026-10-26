$ErrorActionPreference='Stop'
$source=Split-Path -Parent $PSScriptRoot
. "$source\update\github-update.ps1" -LibraryOnly
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$setup=Join-Path $workspace 'outputs\LiteLaunch\1.0.27\LiteLaunch-Setup-1.0.27-x64.exe'
$script:checks=0
function Check($Value,[string]$Name){if(-not $Value){throw "FAIL $Name"};$script:checks++;"PASS $Name"}
function Reject([scriptblock]$Action,[string]$Name){$caught=$false;try{& $Action|Out-Null}catch{$caught=$true};Check $caught $Name}
Check ((Resolve-Repository 'https://github.com/ccl4-56-23-5/LiteLaunch/') -eq 'ccl4-56-23-5/LiteLaunch') 'GitHub project URL normalized'
Check ((Resolve-Repository 'ccl4-56-23-5/LiteLaunch.git') -eq 'ccl4-56-23-5/LiteLaunch') 'owner repo and git suffix accepted'
Reject {Resolve-Repository 'https://example.com/owner/repo'} 'foreign project host rejected'
Reject {Resolve-Repository 'https://github.com/owner/repo?token=anything'} 'query credentials not accepted'
Reject {Resolve-Repository 'owner/..'} 'invalid repository path rejected'
Check ((Get-StableVersion 'v1.0.27') -eq [version]'1.0.27.0') 'stable version normalization'
Reject {Get-StableVersion 'v1.0.27-beta'} 'prerelease version rejected'
$file=Get-Item -LiteralPath $setup;$hash=(Get-FileHash -LiteralPath $setup).Hash
$release=[pscustomobject]@{tag_name='v1.0.27';draft=$false;prerelease=$false;assets=@([pscustomobject]@{name='LiteLaunch-Setup-1.0.27-x64.exe';state='uploaded';size=$file.Length;digest=('sha256:'+$hash);browser_download_url='https://github.com/ccl4-56-23-5/LiteLaunch/releases/download/v1.0.27/LiteLaunch-Setup-1.0.27-x64.exe'})}
$asset=Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch';Assert-Download $setup $asset
Check ($asset.Name -eq 'LiteLaunch-Setup-1.0.27-x64.exe') 'installer asset selected and fully validated'
$release.assets[0].name='LiteLaunch.exe';Reject {Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch'} 'portable EXE never selected';$release.assets[0].name=$asset.Name
$release.draft=$true;Reject {Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch'} 'draft release rejected';$release.draft=$false
$release.prerelease=$true;Reject {Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch'} 'prerelease release rejected';$release.prerelease=$false
$release.assets[0].digest='';Reject {Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch'} 'missing digest rejected';$release.assets[0].digest='sha256:'+$hash
$release.assets[0].browser_download_url='https://github.com/other/repo/releases/download/v1.0.27/'+$asset.Name;Reject {Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch'} 'installer from another repo rejected'
$asset.SHA256='0'*64;Reject {Assert-Download $setup $asset} 'modified installer rejected';$asset.SHA256=$hash
$asset.Version=[version]'1.0.28.0';Reject {Assert-Download $setup $asset} 'wrong installer version rejected';$asset.Version=[version]'1.0.27.0'
$release.assets[0].browser_download_url=$asset.Url
$plan=Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.21'
Check ($plan.UpdateAvailable -and $plan.Asset.Name -eq $asset.Name) 'legacy host resolves installer without requiring LiteLaunch.exe'
$plan=Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.25'
Check ($plan.UpdateAvailable -and $plan.Version -eq [version]'1.0.27.0') 'previous installer host resolves newer release'
$release.assets=@()
Check (-not (Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.27').UpdateAvailable) 'same version does not require an unused asset'
Check (-not (Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.28').UpdateAvailable) 'newer installed version is not downgraded'
Reject {Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.25'} 'newer release without installer is rejected'
$release.assets=@([pscustomobject]@{name=$asset.Name;state='uploaded';size=$file.Length;digest=('sha256:'+$hash);browser_download_url=$asset.Url})
$release.assets+= $release.assets[0]
Reject {Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.25'} 'duplicate installer assets rejected'
$release.assets=@($release.assets[0])
$plugin=[pscustomobject]@{name='LiteLaunch-Plugin-CursorSwitcher-1.0.1-x64.exe';state='uploaded'}
$portable=[pscustomobject]@{name='LiteLaunch.exe';state='uploaded'}
$release.assets=@($plugin,$portable,$release.assets[0])
Check ((Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.25').Asset.Name -eq $asset.Name) 'plugin and portable attachments cannot replace host installer'
$release.assets[2].state='new'
Reject {Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.25'} 'incomplete asset upload rejected'
$release.assets[2].state='uploaded'
$release.assets[2].size=1
Reject {Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.25'} 'abnormal installer size rejected'
$release.assets[2].size=$file.Length
$release.draft=$true
Reject {Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.27'} 'draft rejected even for equal version'
$release.draft=$false
$args=Get-SetupArguments 'D:\Applications With Spaces\LiteLaunch\LiteLaunch.exe' $true
Check ($args.Contains('/DIR="D:\Applications With Spaces\LiteLaunch"')) 'existing install directory correctly quoted'
Check ($args.Contains('/MERGETASKS="autostart"')) 'enabled startup retained in wizard'
Check ((Get-SetupArguments 'D:\Apps\LiteLaunch\LiteLaunch.exe' $false).Contains('/MERGETASKS="!autostart"')) 'disabled startup retained in wizard'
Check ($args -notmatch 'VERYSILENT|SUPPRESSMSGBOXES|FORCECLOSE|/SILENT') 'normal visible wizard requires user interaction'
Reject {Get-SetupArguments 'D:\LiteLaunch.exe' $false} 'disk-root installation target rejected'
$tokens=$null;$errors=$null;[void][Management.Automation.Language.Parser]::ParseFile("$source\update\github-update.ps1",[ref]$tokens,[ref]$errors);Check ($errors.Count -eq 0) 'updater PowerShell parses cleanly'
"RESULT checks=$script:checks failures=0"
