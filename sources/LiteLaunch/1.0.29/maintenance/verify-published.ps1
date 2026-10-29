$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.29';$repo=Join-Path $source 'repository'
$work=Join-Path $workspace 'temp\LiteLaunch-publish-1.0.29'
New-Item -ItemType Directory -Force -Path $work|Out-Null
. "$PSScriptRoot\github-session.ps1"
$head=(git -C $repo rev-parse HEAD).Trim()
$main=GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/git/ref/heads/main'
$tag=GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/git/ref/tags/v1.0.29'
$tagCommit=$tag.object.sha
if($tag.object.type -eq 'tag'){$tagCommit=(GitHubRequest ('/repos/ccl4-56-23-5/LiteLaunch/git/tags/'+$tag.object.sha)).object.sha}
$release=GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/releases/tags/v1.0.29'
$latest=GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/releases/latest'
if($main.object.sha -ne $head -or $tagCommit -ne $head -or $latest.tag_name -ne 'v1.0.29' -or $release.draft -or $release.prerelease){throw 'Remote source/tag/latest mismatch'}
if(@(git -C $repo status --porcelain).Count){throw 'Local source changed after push'}
$expected=@('LiteLaunch-Setup-1.0.29-x64.exe','LiteLaunch-Plugin-BatchRenamer-1.0.1-x64.exe','LiteLaunch-Plugin-CursorSwitcher-1.0.1-x64.exe','LiteLaunch-plugins.json','LiteLaunch-source-v1.0.29.zip','SHA256SUMS.txt','THIRD-PARTY-NOTICES.txt')
if(@($release.assets).Count -ne $expected.Count){throw 'Unexpected release asset count'}
$verified=@()
foreach($name in $expected){
 $asset=@($release.assets|Where-Object name -ceq $name);if($asset.Count -ne 1){throw ('Asset missing/duplicated: '+$name)};$asset=$asset[0]
 $local=if($name -like '*source*.zip'){Join-Path $source $name}else{Join-Path $out $name}
 $hash=(Get-FileHash -LiteralPath $local).Hash
 if($asset.digest -ine ('sha256:'+$hash) -or $asset.size -ne (Get-Item -LiteralPath $local).Length){throw ('Asset metadata mismatch: '+$name)}
 $path=Join-Path $work $name
 Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $path -TimeoutSec 120 -Headers @{'User-Agent'='LiteLaunch-Public-Verification'}
 if((Get-FileHash -LiteralPath $path).Hash -ne $hash -or (Get-Item -LiteralPath $path).Length -ne $asset.size){throw ('Actual downloaded bytes differ: '+$name)}
 $verified+=@{name=$name;sha256=$hash;bytes=$asset.size;downloadVerified=$true;url=$asset.browser_download_url}
 'VERIFIED '+$name
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$tracked=@(((git -C $repo ls-files -z) -join "`n").Split([char]0)|Where-Object {$_})
$tree=@{}
foreach($entry in (((git -C $repo ls-tree -rz --full-tree HEAD) -join "`n").Split([char]0)|Where-Object {$_})){
 if($entry -notmatch '^\d{6} blob ([a-f0-9]{40})\t(.+)$'){throw 'Unexpected source tree entry'}
 $tree[$Matches[2]]=$Matches[1]
}
function Assert-SourceArchive([string]$Zip){
 $archive=[IO.Compression.ZipFile]::OpenRead($Zip)
 try{
  $files=@($archive.Entries|Where-Object {$_.Name});if($files.Count -ne $tracked.Count){throw 'Source archive file count differs'}
  foreach($entry in $files){
   $relative=$entry.FullName.Substring($entry.FullName.IndexOf('/')+1)
   if($relative -notin $tracked){throw ('Unexpected archive path: '+$relative)}
   $stream=$entry.Open();$memory=New-Object IO.MemoryStream
   try{$stream.CopyTo($memory);$bytes=$memory.ToArray()}finally{$stream.Dispose();$memory.Dispose()}
   $header=[Text.Encoding]::ASCII.GetBytes(('blob '+$bytes.Length+[char]0))
   $payload=New-Object byte[] ($header.Length+$bytes.Length)
   [Array]::Copy($header,0,$payload,0,$header.Length);[Array]::Copy($bytes,0,$payload,$header.Length,$bytes.Length)
   $sha=[Security.Cryptography.SHA1]::Create()
   try{$blob=([BitConverter]::ToString($sha.ComputeHash($payload))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
   if($blob -ne $tree[$relative]){throw ('Source archive differs from committed Git bytes: '+$relative)}
  }
 }finally{$archive.Dispose()}
}
Assert-SourceArchive (Join-Path $work 'LiteLaunch-source-v1.0.29.zip')
$githubZip=Join-Path $work 'github-tag-source.zip'
Invoke-WebRequest -UseBasicParsing -Uri ('https://api.github.com/repos/ccl4-56-23-5/LiteLaunch/zipball/'+$head) -OutFile $githubZip -TimeoutSec 120 -Headers @{'User-Agent'='LiteLaunch-Public-Verification'}
Assert-SourceArchive $githubZip
$oldLocal=$env:LOCALAPPDATA
try{
 $env:LOCALAPPDATA=Join-Path $work 'isolated-plugin-profile'
 $runtime=Join-Path $work 'plugin-runtime'
 New-Item -ItemType Directory -Force -Path $runtime|Out-Null
 Copy-Item -LiteralPath "$source\plugins\manager.ps1" -Destination (Join-Path $runtime 'manager.ps1') -Force
 . (Join-Path $runtime 'manager.ps1') -LibraryOnly
 $builtin=Read-PluginCatalog (Get-Content -LiteralPath "$source\plugins\catalog.json" -Raw|ConvertFrom-Json)
 $online=Fetch-PluginCatalog
 if($online.Release.tag_name -ne 'v1.0.29'){throw 'Plugin catalog did not resolve current release'}
 $merged=Merge-PluginCatalog $builtin $online.Catalog
 foreach($plugin in $merged.plugins){
  if($plugin.releaseTag -ne 'v1.0.29'){throw 'Plugin catalog still depends on old release'}
  $asset=Get-PluginReleaseAsset $release $plugin.asset
  if($asset.digest -ine ('sha256:'+$plugin.sha256)){throw 'Plugin descriptor digest differs'}
  Assert-PluginBinary (Join-Path $work $plugin.asset) $plugin
 }
}finally{$env:LOCALAPPDATA=$oldLocal}
$result=[ordered]@{version='1.0.29';commit=$head;verifiedUtc=[datetime]::UtcNow.ToString('o');remoteMainMatches=$true;annotatedTagMatches=$true;latestReleaseMatches=$true;sourceArchivesByteMatchTrackedFiles=$true;sourceFileCount=$tracked.Count;assets=$verified;onlinePluginCatalogAndBinariesVerified=$true;pluginsDependOnlyOnCurrentRelease=$true;installerRebuiltAfterAuthorization=$false;installerExecutedByAssistant=$true;oldVersionsRemovalAuthorized=$true;cleanupAllowed=$true}
$result|ConvertTo-Json -Depth 10|Set-Content -LiteralPath (Join-Path $out 'github-verification.json') -Encoding UTF8
@{version='1.0.29';state='published-authorized-remote-verified';userTestConfirmed=$false;directPublishAuthorized=$true;commit=$head;release=$release.html_url;installerSHA256=(Get-FileHash -LiteralPath (Join-Path $out 'LiteLaunch-Setup-1.0.29-x64.exe')).Hash;remoteDownloadVerified=$true;cleanupAllowed=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'publication-status.json') -Encoding UTF8
'PASS remote main, tag, both source archives, all assets and plugin downloads verified'
