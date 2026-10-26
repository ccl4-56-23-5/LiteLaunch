param([switch]$Recycle)
$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..").TrimEnd('\')
if($source -ine (Join-Path $workspace 'sources\LiteLaunch\1.0.27')){throw 'Unexpected cleanup workspace structure'}
$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.27';$repo=Join-Path $source 'repository'
$proof=Get-Content -LiteralPath (Join-Path $out 'github-verification.json') -Raw|ConvertFrom-Json
$accepted=Get-Content -LiteralPath (Join-Path $out 'user-test-confirmation.json') -Raw|ConvertFrom-Json
if(-not $proof.cleanupAllowed -or -not $proof.onlinePluginCatalogAndBinariesVerified -or -not $accepted.oldVersionsRemovalAuthorized -or @($proof.assets|Where-Object {-not $_.downloadVerified}).Count){throw 'Cleanup is not authorized or remote verification is incomplete'}
if((git -C $repo rev-parse HEAD).Trim() -ne $proof.commit -or @(git -C $repo status --porcelain).Count){throw 'Published Git checkout changed'}
foreach($asset in $proof.assets){$p=if($asset.name -like '*source*.zip'){Join-Path $source $asset.name}else{Join-Path $out $asset.name};if((Get-FileHash -LiteralPath $p).Hash -ne $asset.sha256){throw ('Retained asset changed: '+$asset.name)}}
$old25=Join-Path $workspace 'sources\LiteLaunch\1.0.25\repository'
if((Test-Path -LiteralPath $old25) -and @(git -C $old25 status --porcelain).Count){throw 'Old official worktree has unexpected changes'}
$old26=Join-Path $workspace 'sources\LiteLaunch\1.0.26'
if(Test-Path -LiteralPath $old26){
 $oldRepo=Join-Path $old26 'repository'
 git -C $oldRepo diff --quiet
 if($LASTEXITCODE -ne 0 -or @(git -C $oldRepo ls-files --others --exclude-standard).Count){throw 'Old candidate has unreviewed changes'}
 # Legacy text manifests used ASCII and cannot represent Chinese filenames.
 # Compare the actual original ZIP entries instead of guessing those names.
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $archive=[IO.Compression.ZipFile]::OpenRead((Join-Path $old26 'LiteLaunch-source-v1.0.26.zip'))
 try{foreach($entry in $archive.Entries|Where-Object Name){
  $stream=$entry.Open();$sha=[Security.Cryptography.SHA256]::Create()
  try{$hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','')}finally{$stream.Dispose();$sha.Dispose()}
  if((Get-FileHash -LiteralPath (Join-Path $oldRepo $entry.FullName)).Hash -ne $hash){throw ('Old candidate differs from its prepared archive: '+$entry.FullName)}
 }}finally{$archive.Dispose()}
}
$relative=@('sources\LiteLaunch\1.0.25','sources\LiteLaunch\1.0.26','outputs\LiteLaunch\1.0.25','outputs\LiteLaunch\1.0.26','temp\LiteLaunch-build-1.0.26','temp\LiteLaunch-build-1.0.27','temp\LiteLaunch-installer\payload-1.0.26','temp\LiteLaunch-installer\payload-1.0.27','temp\LiteLaunch-publish-1.0.27','temp\LiteLaunch-installer\finalize-records.ps1','temp\LiteLaunch-installer\prepare-release22.ps1','temp\LiteLaunch-installer\publish22.ps1','temp\LiteLaunch-installer\record-detection-fix.ps1','temp\LiteLaunch-installer\recycle-obsolete.ps1')
$processes=@(Get-CimInstance Win32_Process)
$audit=@()
foreach($rel in $relative){
 $path=[IO.Path]::GetFullPath((Join-Path $workspace $rel)).TrimEnd('\')
 if(-not $path.StartsWith($workspace+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '基线' -or $path -in @($source,$out,$repo)){throw 'Unsafe cleanup path'}
 if(-not (Test-Path -LiteralPath $path)){continue}
 $item=Get-Item -LiteralPath $path -Force
 if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Cleanup target is a reparse point'}
 $entries=if($item.PSIsContainer){@(Get-ChildItem -LiteralPath $path -Recurse -Force)}else{@($item)}
 if(@($entries|Where-Object {($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $_.Name -match '基线'}).Count){throw ('Protected nested content: '+$path)}
 if(@($processes|Where-Object {$_.ExecutablePath -and ($_.ExecutablePath -ieq $path -or $_.ExecutablePath.StartsWith($path+'\',[StringComparison]::OrdinalIgnoreCase))}).Count){throw ('Cleanup target is in use: '+$path)}
 $files=@($entries|Where-Object {-not $_.PSIsContainer})
 $audit+=@{path=$path;directory=$item.PSIsContainer;files=$files.Count;bytes=[long](($files|Measure-Object Length -Sum).Sum);registeredBaseline=$false;reparsePointsAbsent=$true;runningExecutablesAbsent=$true}
}
$audit|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $out 'cleanup-targets.json') -Encoding UTF8
'Audited cleanup targets: '+$audit.Count
if(-not $Recycle){return}
function Hash-Files($Files){$map=@{};foreach($file in $Files){$map[$file.FullName]=@{sha256=(Get-FileHash -LiteralPath $file.FullName).Hash;bytes=$file.Length}};return $map}
$protected=@()
$profile=Join-Path $env:LOCALAPPDATA 'LiteLaunch'
foreach($name in 'LiteLaunch.ini','LiteLaunch.ini.bak','LiteLaunch.initial.ini'){$protected+=Get-Item -LiteralPath (Join-Path $profile $name)}
foreach($folder in 'Plugins','Rename'){$p=Join-Path $profile $folder;if(Test-Path -LiteralPath $p){$protected+=Get-ChildItem -LiteralPath $p -Recurse -Force -File}}
$installed=& "$source\qa\inspect-existing-install.ps1"
if($installed.ExpectedUpgradeDirectory){$protected+=Get-Item -LiteralPath (Join-Path $installed.ExpectedUpgradeDirectory 'LiteLaunch.exe')}
foreach($rel in 'temp\LiteLaunch-add-baidu-20260923-173656','temp\LiteLaunch-config-migration','temp\LiteLaunch-deploy-1.0.21','temp\LiteLaunch-installer\unpack-tool'){$p=Join-Path $workspace $rel;if(Test-Path -LiteralPath $p){$protected+=Get-ChildItem -LiteralPath $p -Recurse -Force -File}}
$protectedHash=Hash-Files $protected
$protectedHash|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $out 'cleanup-protected-before.json') -Encoding UTF8
$retained=@(Get-ChildItem -LiteralPath $source -Recurse -Force -File|Where-Object {$_.FullName -notlike '*\repository\.git\*'})+@(Get-ChildItem -LiteralPath $out -Force -File)
$retainedHash=Hash-Files $retained
$retainedHash|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $out 'cleanup-retained-before.json') -Encoding UTF8
Add-Type -AssemblyName Microsoft.VisualBasic
foreach($item in $audit){
 $path=[IO.Path]::GetFullPath($item.path).TrimEnd('\')
 if($path -cne $item.path -or -not $path.StartsWith($workspace+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup path resolution changed'}
 if($item.directory){[Microsoft.VisualBasic.FileIO.FileSystem]::DeleteDirectory($path,[Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs,[Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)}else{[Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($path,[Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs,[Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)}
 if(Test-Path -LiteralPath $path){throw ('Cleanup target remains: '+$path)}
 'RECYCLED '+$path
}
foreach($map in @($protectedHash,$retainedHash)){foreach($p in $map.Keys){if(-not (Test-Path -LiteralPath $p -PathType Leaf) -or (Get-FileHash -LiteralPath $p).Hash -ne $map[$p].sha256){throw ('Protected/retained file changed: '+$p)}}}
foreach($base in 'sources\LiteLaunch','outputs\LiteLaunch'){$versions=@(Get-ChildItem -LiteralPath (Join-Path $workspace $base) -Directory|Where-Object {$_.Name -notmatch '基线'});if($versions.Count -ne 1 -or $versions[0].Name -ne '1.0.27'){throw 'Unexpected remaining ordinary versions'}}
if(@(git -C $repo status --porcelain).Count){throw 'Published Git checkout became dirty'}
@{version='1.0.27';completedUtc=[datetime]::UtcNow.ToString('o');method='Windows Recycle Bin';targets=$audit;allTargetsAbsent=$true;retainedFilesUnchanged=$retainedHash.Count;protectedUserAndRecoveryFilesUnchanged=$protectedHash.Count;localOrdinaryVersions=@('1.0.27');registeredBaselinesPreserved=$true;publishedWorktreeClean=$true}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $out '清理记录.json') -Encoding UTF8
'PASS only latest ordinary source/output remain; published artifacts and user/recovery files unchanged'
