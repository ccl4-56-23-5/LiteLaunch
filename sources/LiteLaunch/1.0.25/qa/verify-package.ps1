$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.25'
$launch=Get-Content -LiteralPath (Join-Path $out 'launch-verification.json') -Raw|ConvertFrom-Json
if(-not $launch.oldHiddenStartupReproduced -or -not $launch.firstClickInstallsVerifiedAdjacentPackageAndShowsNativeRename -or -not $launch.installedClickShowsNativeRename -or -not $launch.missingPluginManagerVisibleAndRequestedPluginSelected){throw 'Native launch acceptance record incomplete'}
foreach($entry in @(@('cursor-tests.log','RESULT checks=18 failures=0'),@('cursor-tests.log','RESULT checks=35 failures=0'),@('plugin-tests.log','RESULT checks=22 failures=0'))){if(-not (Select-String -LiteralPath (Join-Path $out $entry[0]) -SimpleMatch $entry[1])){throw ('Required checks missing: '+$entry[0])}}
$live=Get-Content -LiteralPath (Join-Path $out 'cursor-live-verification.json') -Raw|ConvertFrom-Json
if(-not $live.loadedArrowPixelsChanged -or -not $live.originalValuesAndTypesRestored -or -not $live.originalArrowPixelsRestored){throw 'Real cursor switch/restore record incomplete'}
$previous=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'previous-release-evidence.json') -Raw|ConvertFrom-Json
foreach($property in $previous.files.PSObject.Properties){if((Get-FileHash -LiteralPath (Join-Path $source $property.Name)).Hash -ne $property.Value){throw ('Inherited rename evidence invalid for changed source: '+$property.Name)}}
$payload=Join-Path $workspace 'temp\LiteLaunch-installer\payload-1.0.25'
$setup=Join-Path $out 'LiteLaunch-Setup-1.0.25-x64.exe'
$audit=Join-Path $workspace ('temp\LiteLaunch-build-1.0.25\package-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $audit|Out-Null
& (Join-Path $workspace 'temp\LiteLaunch-installer\unpack-tool\files\innounp.exe') -x -y "-d$audit" $setup *> (Join-Path $audit 'extraction.log')
if($LASTEXITCODE -ne 0){throw 'Installer data extraction failed'}
$main=Get-ChildItem -LiteralPath $audit -Recurse -File -Filter 'LiteLaunch.exe'
if(@($main).Count -ne 1){throw 'Installer main payload missing or duplicated'}
$mainHash=(Get-FileHash -LiteralPath $main.FullName).Hash
if($mainHash -ne (Get-FileHash -LiteralPath (Join-Path $payload 'LiteLaunch.exe')).Hash){throw 'Installer payload does not match final host'}
if(Get-ChildItem -LiteralPath $audit -Recurse -File -Filter 'LiteLaunch-Plugin-*.exe'){throw 'Optional plugin unexpectedly bundled'}
if(-not ('PackageResources' -as [type])){Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class PackageResources {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr LoadLibraryEx(string path,IntPtr file,uint flags);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr FindResource(IntPtr module,IntPtr name,IntPtr type);
 [DllImport("kernel32.dll")] public static extern uint SizeofResource(IntPtr module,IntPtr resource);
 [DllImport("kernel32.dll")] public static extern IntPtr LoadResource(IntPtr module,IntPtr resource);
 [DllImport("kernel32.dll")] public static extern IntPtr LockResource(IntPtr resource);
 [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
}
'@}
function Resource([string]$path,[int]$id,[int]$type=10){
 $module=[PackageResources]::LoadLibraryEx($path,[IntPtr]::Zero,0x22)
 if($module -eq [IntPtr]::Zero){throw 'Cannot open resource data'}
 try{$found=[PackageResources]::FindResource($module,[IntPtr]$id,[IntPtr]$type);if($found -eq [IntPtr]::Zero){return $null};$bytes=New-Object byte[] ([PackageResources]::SizeofResource($module,$found));[Runtime.InteropServices.Marshal]::Copy([PackageResources]::LockResource([PackageResources]::LoadResource($module,$found)),$bytes,0,$bytes.Length);return ,$bytes}finally{[void][PackageResources]::FreeLibrary($module)}
}
function Digest([byte[]]$bytes){$algorithm=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace('-','')}finally{$algorithm.Dispose()}}
$resources=@{107='icons\plugin-renamer.svg';108='icons\plugin-cursor.svg';109='icons\plugin-add.svg';110='plugins\renamer.ico';111='plugins\cursor.ico';302='update\github-update.ps1';303='plugins\manager.ps1';304='plugins\catalog.json'}
foreach($id in $resources.Keys){$bytes=Resource $main.FullName $id;if($null -eq $bytes -or (Digest $bytes) -ne (Get-FileHash -LiteralPath (Join-Path $source $resources[$id])).Hash){throw "Embedded resource mismatch: $id"}}
if($null -ne (Resource $main.FullName 301)){throw 'Host still contains rename worker'}
$catalog=Get-Content -LiteralPath (Join-Path $out 'LiteLaunch-plugins.json') -Raw|ConvertFrom-Json
if((Get-FileHash -LiteralPath (Join-Path $out 'LiteLaunch-plugins.json')).Hash -ne (Get-FileHash -LiteralPath (Join-Path $source 'plugins\catalog.json')).Hash){throw 'Output catalog mismatch'}
$plugins=@(foreach($entry in $catalog.plugins){
 $file=Get-Item -LiteralPath (Join-Path $out $entry.asset);$hash=(Get-FileHash -LiteralPath $file.FullName).Hash
 if($hash -ne $entry.sha256 -or $file.VersionInfo.ProductName -ne $entry.productName -or $file.VersionInfo.FileVersion -ne ($entry.version+'.0')){throw ('Plugin identity mismatch: '+$entry.id)}
 if($entry.id -eq 'batch-renamer'){foreach($id in @(301,400)){if($null -eq (Resource $file.FullName $id)){throw 'Batch worker or notices missing'}};if($null -eq (Resource $file.FullName 1 24)){throw 'Batch manifest missing'}}
 [ordered]@{id=$entry.id;version=$entry.version;asset=$entry.asset;bytes=$file.Length;sha256=$hash}
})
if($main.VersionInfo.FileVersion -notlike '1.0.25*'){throw 'Wrong host version'}
$record=[ordered]@{version='1.0.25';state='local-candidate-awaiting-user-test';verifiedUtc=[DateTime]::UtcNow.ToString('o');setup=[ordered]@{file=(Split-Path $setup -Leaf);bytes=(Get-Item $setup).Length;sha256=(Get-FileHash -LiteralPath $setup).Hash;payloadSha256=$mainHash};plugins=$plugins;checks=[ordered]@{installerPayloadMatches=$true;pluginsNotBundled=$true;embeddedCatalogAndScriptsMatch=$true;embeddedPluginIconsMatch=$true;hostRenameWorkerAbsent=$true;batchWorkerLicenseAndManifestPresent=$true;cursorEngine=18;cursorUi=35;realCursorApplyAndRestore=$true;pluginManager=22;nativeLaunchScenarios=5;unchangedRenameEvidenceFromVersion24=@{batchPipeline=27;batchForms=28};cursorResponsiveLayouts=$true};notPerformed=@('installer execution','live GitHub download/update','user visual acceptance');extractedTo=$audit}
$record|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $out 'verification.json') -Encoding utf8
Get-ChildItem -LiteralPath $out -File|Where-Object {$_.Extension -eq '.exe' -or $_.Name -eq 'LiteLaunch-plugins.json'}|Sort-Object Name|ForEach-Object {((Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant())+'  '+$_.Name}|Set-Content -LiteralPath (Join-Path $out 'SHA256SUMS.txt') -Encoding ascii
'PASS installer data, host resources, plugin identities and final catalog verified without installation'
