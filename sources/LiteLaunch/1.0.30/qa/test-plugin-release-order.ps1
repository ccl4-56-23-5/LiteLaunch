$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$work=Join-Path $workspace 'temp\LiteLaunch-build-1.0.30\release-order-qa'
New-Item -ItemType Directory -Force -Path $work|Out-Null
Copy-Item -LiteralPath "$source\plugins\manager.ps1" -Destination "$work\manager.ps1" -Force
. "$work\manager.ps1" -LibraryOnly
$asset=[pscustomobject]@{name='LiteLaunch-plugins.json'}
$old=[pscustomobject]@{tag_name='v1.0.29';published_at='2026-10-29T06:54:11Z';draft=$false;prerelease=$false;assets=@($asset)}
$new=[pscustomobject]@{tag_name='batch-renamer-v1.0.2';published_at='2026-10-29T07:52:55Z';draft=$false;prerelease=$false;assets=@($asset)}
$draft=[pscustomobject]@{tag_name='draft';published_at='2027-01-01T00:00:00Z';draft=$true;prerelease=$false;assets=@($asset)}
$pre=[pscustomobject]@{tag_name='pre';published_at='2027-01-01T00:00:00Z';draft=$false;prerelease=$true;assets=@($asset)}
$noCatalog=[pscustomobject]@{tag_name='other';published_at='2027-01-01T00:00:00Z';draft=$false;prerelease=$false;assets=@()}
$checks=0
function Check($ok,$name){if(-not $ok){throw "FAIL $name"};$script:checks++;"PASS $name"}
$ordered=@(Get-PluginCatalogReleases @($old,$new))
Check ($ordered[0].tag_name -eq $new.tag_name) 'newer independent plugin precedes marked-latest host'
Check ((@(Get-PluginCatalogReleases @($new,$old)))[0].tag_name -eq $new.tag_name) 'selection does not depend on GitHub list order'
$ordered=@(Get-PluginCatalogReleases @($draft,$pre,$noCatalog,$old,$new))
Check ($ordered.Count -eq 2) 'draft prerelease and releases without catalog excluded'
Check ($ordered[1].tag_name -eq $old.tag_name) 'older stable catalog remains ordered fallback'
$previous=$env:LOCALAPPDATA
try{$env:LOCALAPPDATA=Join-Path $work ('isolated-'+[guid]::NewGuid().ToString('N'));$script:PluginRoot=Join-Path $env:LOCALAPPDATA 'LiteLaunch\Plugins';$actual=Fetch-PluginCatalog;Check ($actual.Release.tag_name -eq 'batch-renamer-v1.0.2' -and @($actual.Catalog.plugins|Where-Object {$_.id -eq 'batch-renamer' -and $_.version -eq '1.0.2'}).Count -eq 1) 'actual GitHub discovery finds independent batch 1.0.2 while host remains latest'}finally{$env:LOCALAPPDATA=$previous}
'RESULT checks='+$checks+' failures=0'
