param([switch]$Recycle)
$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$hostSource=Join-Path $workspace 'sources\LiteLaunch\1.0.30';$hostOut=Join-Path $workspace 'outputs\LiteLaunch\1.0.30'
$batchSource=Join-Path $workspace 'sources\LiteLaunchBatchRenamer\1.0.2';$batchOut=Join-Path $workspace 'outputs\LiteLaunchBatchRenamer\1.0.2';$repo=Join-Path $hostSource 'repository'
$hostProof=Get-Content (Join-Path $hostOut 'github-verification.json') -Raw|ConvertFrom-Json
$batchProof=Get-Content (Join-Path $batchOut 'github-verification.json') -Raw|ConvertFrom-Json
if(-not $hostProof.cleanupAllowed -or -not $batchProof.cleanupAllowed -or -not $batchProof.sourceArchivesMatchCommittedBlobs -or -not $hostProof.sourceArchivesByteMatchTrackedFiles){throw 'Remote verification is incomplete'}
if(@(git -C $repo status --porcelain).Count -or (git -C $repo rev-parse HEAD).Trim() -ne $hostProof.commit){throw 'Published Git checkout changed'}
foreach($record in $hostProof.assets){$path=if($record.name -like '*source*.zip'){Join-Path $hostSource $record.name}else{Join-Path $hostOut $record.name};if((Get-FileHash -LiteralPath $path).Hash -ine $record.sha256){throw 'Current host delivery changed'}}
foreach($record in $batchProof.assets){$path=if($record.name -like '*source*.zip'){Join-Path $batchSource $record.name}else{Join-Path $batchOut $record.name};if((Get-FileHash -LiteralPath $path).Hash -ine $record.sha256){throw 'Current plugin delivery changed'}}
$relative=@('sources\LiteLaunch\1.0.29','outputs\LiteLaunch\1.0.29','sources\LiteLaunchBatchRenamer\1.0.1','outputs\LiteLaunchBatchRenamer\1.0.1','sources\LiteLaunchBatchRenamer\1.0.2\LiteLaunch-source-batch-renamer-v1.0.2.zip')
$targets=@();$processes=@(Get-CimInstance Win32_Process)
foreach($rel in $relative){
 $path=[IO.Path]::GetFullPath((Join-Path $workspace $rel));if(-not $path.StartsWith($workspace+'\',[StringComparison]::OrdinalIgnoreCase) -or $path -match '基线' -or $path -in @($hostSource,$batchSource,$hostOut,$batchOut,$repo)){throw 'Cleanup scope mismatch'}
 $item=Get-Item -LiteralPath $path -Force;$children=if($item.PSIsContainer){@(Get-ChildItem -LiteralPath $path -Force -Recurse)}else{@($item)}
 if(($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or @($children|Where-Object {($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $_.Name -match '基线' -or $_.Name -eq '.git'}).Count){throw 'Protected baseline, worktree or reparse point found'}
 if(@($processes|Where-Object {$_.ExecutablePath -and ($_.ExecutablePath -ieq $path -or $_.ExecutablePath.StartsWith($path+'\',[StringComparison]::OrdinalIgnoreCase))}).Count){throw 'Cleanup target has active executable'}
 if($item.PSIsContainer -and $rel.StartsWith('sources\')){foreach($line in Get-Content -LiteralPath (Join-Path $path 'SOURCE-SHA256SUMS.txt')){if($line -notmatch '^([A-Fa-f0-9]{64})  (.+)$'){throw 'Old source manifest invalid'};$hash=$Matches[1];$name=$Matches[2];$file=Join-Path $path $name;if(-not (Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file).Hash -ine $hash){throw ('Old source has unrecorded changes: '+$name)}}}
 $files=@($children|Where-Object {-not $_.PSIsContainer});$targets+=@{path=$path;directory=$item.PSIsContainer;files=$files.Count;bytes=[long](($files|Measure-Object Length -Sum).Sum)}
}
$targets|ConvertTo-Json -Depth 6|Set-Content (Join-Path $hostOut 'cleanup-targets.json') -Encoding UTF8
if(-not $Recycle){'PASS precise local cleanup targets audited';return}
$retained=@{};foreach($root in @($hostSource,$hostOut,$batchSource,$batchOut)){foreach($file in Get-ChildItem -LiteralPath $root -Force -Recurse -File){if($file.FullName -like "$repo\.git\*" -or $file.FullName -in $targets.path){continue};$retained[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash}}
$profile=Join-Path $env:LOCALAPPDATA 'LiteLaunch';$userFiles=@(Get-ChildItem -LiteralPath $profile -Recurse -File|Where-Object {$_.FullName -notmatch '\\(PluginSessions|Logs|Runtime)\\' -and $_.Extension -in '.ini','.json','.bin','.llrules','.bak'})+@(Get-Item (Get-Content (Join-Path $hostOut 'local-install-verification.json') -Raw|ConvertFrom-Json).path)
$userHashes=@{};foreach($file in $userFiles){$userHashes[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash}
Add-Type -AssemblyName Microsoft.VisualBasic
foreach($target in $targets){$path=[IO.Path]::GetFullPath($target.path);if($path -cne $target.path -or -not $path.StartsWith($workspace+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Resolved target changed'};if($target.directory){[Microsoft.VisualBasic.FileIO.FileSystem]::DeleteDirectory($path,[Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs,[Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)}else{[Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($path,[Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs,[Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)};if(Test-Path -LiteralPath $path){throw 'Recycle did not remove target'};'RECYCLED '+$path}
foreach($map in @($retained,$userHashes)){foreach($path in $map.Keys){if((Get-FileHash -LiteralPath $path).Hash -ne $map[$path]){throw 'Retained source, delivery or user file changed'}}}
if(@(git -C $repo status --porcelain).Count){throw 'Git checkout changed during cleanup'}
[ordered]@{utc=[DateTime]::UtcNow.ToString('o');method='Windows Recycle Bin';targets=$targets;oldPathsAbsent=$true;retainedFilesUnchanged=$retained.Count;userFilesUnchanged=$userHashes.Count;baselinesPreserved=$true;gitHistoryPreserved=$true;hostVersion='1.0.30';batchVersion='1.0.2';duplicatePluginSourceZipReplacedByCurrentFullRepositoryZip=$true}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $hostOut 'cleanup-verification.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $hostOut 'cleanup-verification.json') -Destination (Join-Path $batchOut 'cleanup-verification.json') -Force
'PASS local retention now contains current host and current plugin only'
