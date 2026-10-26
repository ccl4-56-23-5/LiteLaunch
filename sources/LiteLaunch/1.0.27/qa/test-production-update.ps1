param([switch]$Online)
$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.27'
. "$source\update\github-update.ps1" -LibraryOnly
$release=Get-Content -LiteralPath "$PSScriptRoot\production-release-1.0.25.json" -Raw|ConvertFrom-Json
if($Online){
 [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
 $release=Invoke-RestMethod -Uri 'https://api.github.com/repos/ccl4-56-23-5/LiteLaunch/releases/latest' -Headers @{'User-Agent'='LiteLaunch-Updater-QA';Accept='application/vnd.github+json'} -TimeoutSec 25
}
# The exact 1.0.21 asset predicate that caused the reported error.
$legacyAssets=@($release.assets|Where-Object {$_.name -ceq 'LiteLaunch.exe' -and $_.state -eq 'uploaded'})
if($legacyAssets.Count -ne 0){throw 'Legacy failure fixture changed'}
'PASS legacy 1.0.21 predicate reproduced missing LiteLaunch.exe against installer-only release'
$plan=Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.21'
if(-not $plan.UpdateAvailable -or $plan.Asset.Name -notlike 'LiteLaunch-Setup-*-x64.exe'){throw 'Installer migration failed'}
'PASS same real release resolves verified Setup for legacy and installer hosts'
$plan24=Get-UpdatePlan $release 'ccl4-56-23-5/LiteLaunch' '1.0.24'
if($plan24.Asset.Name -ne $plan.Asset.Name){throw 'Previous host migration differs'}
$downloadValidated=$false
if($Online){
 $dir=Join-Path $workspace 'temp\LiteLaunch-build-1.0.27\online-update'
 New-Item -ItemType Directory -Force -Path $dir|Out-Null
 $path=Join-Path $dir $plan.Asset.Name
 Invoke-WebRequest -UseBasicParsing -Uri $plan.Asset.Url -OutFile $path -TimeoutSec 90 -Headers @{'User-Agent'='LiteLaunch-Updater-QA'}
 Assert-Download $path $plan.Asset
 $downloadValidated=$true
 'PASS actual production installer downloaded and product/version/size/SHA-256 validated without execution'
}
$recordName=if($Online){'online-update-verification.json'}else{'production-fixture-verification.json'}
@{checkedUtc=[datetime]::UtcNow.ToString('o');release=$release.tag_name;liveMetadataFetched=[bool]$Online;legacyMissingExeReproduced=$true;newUpdaterChoosesSetup=$true;previousHostChoosesSetup=$true;actualInstallerDownloadedAndValidated=$downloadValidated;installerExecuted=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out $recordName) -Encoding UTF8
