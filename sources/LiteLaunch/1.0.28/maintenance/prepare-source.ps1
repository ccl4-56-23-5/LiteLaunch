$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$repo=Join-Path $source 'repository';$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.28'
$work=Join-Path $workspace 'temp\LiteLaunch-publish-1.0.28'
New-Item -ItemType Directory -Force -Path $work|Out-Null
if(@(git -C $repo status --porcelain).Count){throw 'Source checkout is not clean'}
$head=(git -C $repo rev-parse HEAD).Trim()
$zipPath=Join-Path $work 'source-prepared.zip'
git -C $repo archive --format=zip --prefix=LiteLaunch-v1.0.28/ -o $zipPath HEAD
if($LASTEXITCODE -ne 0){throw 'Source ZIP creation failed'}
$tree=@{}
foreach($row in (((git -C $repo ls-tree -rz --full-tree HEAD) -join "`n").Split([char]0)|Where-Object {$_})){
 if($row -notmatch '^\d{6} blob ([a-f0-9]{40})\t(.+)$'){throw 'Unexpected Git tree entry'}
 $tree[$Matches[2]]=$Matches[1]
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($zipPath);$manifest=@()
try{
 $files=@($archive.Entries|Where-Object Name)
 if($files.Count -ne $tree.Count){throw 'Source ZIP file count differs'}
 foreach($entry in $files){
  $relative=$entry.FullName.Substring($entry.FullName.IndexOf('/')+1)
  $stream=$entry.Open();$memory=New-Object IO.MemoryStream
  try{$stream.CopyTo($memory);$bytes=$memory.ToArray()}finally{$stream.Dispose();$memory.Dispose()}
  $header=[Text.Encoding]::ASCII.GetBytes(('blob '+$bytes.Length+[char]0));$payload=New-Object byte[] ($header.Length+$bytes.Length)
  [Array]::Copy($header,0,$payload,0,$header.Length);[Array]::Copy($bytes,0,$payload,$header.Length,$bytes.Length)
  $sha1=[Security.Cryptography.SHA1]::Create()
  try{$blob=([BitConverter]::ToString($sha1.ComputeHash($payload))).Replace('-','').ToLowerInvariant()}finally{$sha1.Dispose()}
  if($blob -ne $tree[$relative]){throw ('Source ZIP differs from Git object: '+$relative)}
  $sha256=[Security.Cryptography.SHA256]::Create()
  try{$hash=([BitConverter]::ToString($sha256.ComputeHash($bytes))).Replace('-','').ToLowerInvariant()}finally{$sha256.Dispose()}
  $manifest+=($hash+'  '+$relative)
 }
}finally{$archive.Dispose()}
$destination=Join-Path $source 'LiteLaunch-source-v1.0.28.zip'
if(Test-Path -LiteralPath $destination){throw 'Final source archive already exists; inspect before replacing'}
Move-Item -LiteralPath $zipPath -Destination $destination
$manifest|Set-Content -LiteralPath (Join-Path $source 'SOURCE-SHA256SUMS.txt') -Encoding UTF8
$assets=@(Get-ChildItem -LiteralPath $out -File|Where-Object {$_.Extension -eq '.exe' -or $_.Name -in 'LiteLaunch-plugins.json','THIRD-PARTY-NOTICES.txt'})+@(Get-Item -LiteralPath $destination)
$assets|Sort-Object Name|ForEach-Object {((Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant())+'  '+$_.Name}|Set-Content -LiteralPath (Join-Path $out 'SHA256SUMS.txt') -Encoding UTF8
$authorization=Get-Content -LiteralPath (Join-Path $out 'release-authorization.json') -Raw|ConvertFrom-Json
if(-not $authorization.directPublishAuthorized -or $authorization.installerSHA256 -ne (Get-FileHash -LiteralPath (Join-Path $out 'LiteLaunch-Setup-1.0.28-x64.exe')).Hash){throw 'Direct publication authorization/build mismatch'}
$authorization|Add-Member -Force NoteProperty publishedSourceCommit $head
$authorization|Add-Member -Force NoteProperty publishedSourceZIPHash ((Get-FileHash -LiteralPath $destination).Hash)
$authorization|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $out 'release-authorization.json') -Encoding UTF8
@{version='1.0.28';files=$tree.Count;sourceCommit=$head;zipSHA256=$authorization.publishedSourceZIPHash;allZipEntriesMatchCommittedGitObjects=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'public-source-verification.json') -Encoding UTF8
'PASS source ZIP matches '+$tree.Count+' committed files'
