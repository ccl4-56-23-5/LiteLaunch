param([string]$Extractor="$PSScriptRoot\..\..\..\..\temp\LiteLaunch-installer\unpack-tool\files\innounp.exe")
$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.30'
$setup=Join-Path $out 'LiteLaunch-Setup-1.0.30-x64.exe'
$payload=Join-Path $workspace 'temp\LiteLaunch-installer\payload-1.0.30\LiteLaunch.exe'
$audit=Join-Path $workspace ('temp\LiteLaunch-build-1.0.30\package-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $audit|Out-Null
& $Extractor -x -y "-d$audit" $setup *> (Join-Path $audit 'extraction.log')
if($LASTEXITCODE -ne 0){throw 'Installer data extraction failed'}
$main=@(Get-ChildItem -LiteralPath $audit -Recurse -File -Filter LiteLaunch.exe)
if($main.Count -ne 1){throw 'Main application missing or duplicated'}
if((Get-FileHash -LiteralPath $main[0].FullName).Hash -ne (Get-FileHash -LiteralPath $payload).Hash){throw 'Installer contains different application'}
if(Get-ChildItem -LiteralPath $audit -Recurse -File -Filter 'LiteLaunch-Plugin-*.exe'){throw 'Optional plugin unexpectedly bundled'}
$bridge=@(Get-ChildItem -LiteralPath $audit -Recurse -File -Filter 'LiteLaunch-UpdateBridge.dll')
if($bridge.Count -ne 1){throw 'Installer shutdown bridge missing or duplicated'}
$bridgeHash=(Get-FileHash -LiteralPath $bridge[0].FullName).Hash
if($bridgeHash -ne (Get-FileHash -LiteralPath (Join-Path $workspace 'temp\LiteLaunch-build-1.0.30\LiteLaunch-UpdateBridge.dll')).Hash){throw 'Embedded shutdown bridge differs from tested build'}
$bridgeBytes=[IO.File]::ReadAllBytes($bridge[0].FullName)
$peOffset=[BitConverter]::ToInt32($bridgeBytes,0x3c)
if([BitConverter]::ToUInt16($bridgeBytes,$peOffset+4) -ne 0x14c){throw 'Bridge must match 32-bit Setup engine'}
$shutdown=Get-Content -LiteralPath (Join-Path $out 'shutdown-verification.json') -Raw|ConvertFrom-Json
if($shutdown.failures -ne 0 -or $shutdown.bridgeSHA256 -ne $bridgeHash -or -not $shutdown.privateFixtureOnly){throw 'Matching shutdown bridge regression evidence missing'}
& "$PSScriptRoot\assert-updater-embedded.ps1" -ApplicationExe $main[0].FullName
. "$PSScriptRoot\package-resources.ps1"
foreach($pair in @(@(303,'plugins\manager.ps1'),@(304,'plugins\catalog.json'))){
 if((Get-BytesSHA256 (Get-PackageResource $main[0].FullName $pair[0])) -ne (Get-FileHash -LiteralPath (Join-Path $source $pair[1])).Hash){throw ('Embedded plugin resource differs: '+$pair[0])}
}
$embeddedScript=Join-Path $audit 'packaged-updater.ps1'
[IO.File]::WriteAllBytes($embeddedScript,(Get-PackageResource $main[0].FullName 302))
. $embeddedScript -LibraryOnly
$release=Get-Content -LiteralPath "$PSScriptRoot\production-release-1.0.25.json" -Raw|ConvertFrom-Json
$plan=Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.21'
if(-not $plan.UpdateAvailable -or $plan.Asset.Name -ne 'LiteLaunch-Setup-1.0.25-x64.exe'){throw 'Packaged updater cannot migrate legacy versions to real production release'}
if((Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.30').UpdateAvailable){throw 'Packaged updater attempts downgrade'}
$catalog=Get-Content -LiteralPath "$source\plugins\catalog.json" -Raw|ConvertFrom-Json
foreach($plugin in $catalog.plugins){if((Get-FileHash -LiteralPath (Join-Path $out $plugin.asset)).Hash -ine $plugin.sha256){throw 'Preserved plugin digest differs'}}
$record=[ordered]@{version='1.0.30';state='built-and-verified-awaiting-installation';verifiedUtc=[datetime]::UtcNow.ToString('o');setup=[ordered]@{file=(Split-Path $setup -Leaf);bytes=(Get-Item $setup).Length;sha256=(Get-FileHash -LiteralPath $setup).Hash;payloadSha256=(Get-FileHash -LiteralPath $main[0].FullName).Hash};checks=@{installerExtractedWithoutExecution=$true;payloadMatches=$true;embeddedUpdaterMatchesSource=$true;packagedUpdaterAcceptsProductionInstallerFromLegacyVersion=$true;packagedUpdaterRejectsDowngrade=$true;pluginResourcesMatch=$true;existingPublishedPluginsPreserved=$true};notPerformed=@('user acceptance of version 1.0.30','live installation through in-app updater','GitHub publication of version 1.0.30');extractedTo=$audit}
$record|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $out 'verification.json') -Encoding UTF8
@{bridgeSHA256=$bridgeHash;bits=32;privateChecks=$shutdown.checks;embeddedBridgeMatchesTestedDLL=$true;installerExecuted=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'installer-shutdown-package.json') -Encoding UTF8
'PASS extracted installer payload, actual embedded updater and published plugin hashes'
