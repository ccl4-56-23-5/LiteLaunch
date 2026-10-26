$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.27'
$repo=Join-Path $source 'repository'
. "$PSScriptRoot\github-session.ps1"
$accepted=Get-Content -LiteralPath (Join-Path $out 'user-test-confirmation.json') -Raw|ConvertFrom-Json
$setup=Join-Path $out 'LiteLaunch-Setup-1.0.27-x64.exe'
if($accepted.version -ne '1.0.27' -or $accepted.installerSHA256 -ne (Get-FileHash -LiteralPath $setup).Hash -or $accepted.installerRebuiltAfterAcceptance){throw 'Accepted installer identity changed'}
if(@(git -C $repo status --porcelain).Count){throw 'Git worktree is not clean'}
if((git -C $repo remote get-url origin).Trim() -cne 'https://github.com/ccl4-56-23-5/LiteLaunch.git'){throw 'Unexpected remote'}
$head=(git -C $repo rev-parse HEAD).Trim()
if((git -C $repo rev-parse 'v1.0.27^{}').Trim() -ne $head){throw 'Tag does not match HEAD'}
$remote=GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/git/ref/heads/main'
$parent=(git -C $repo rev-parse 'HEAD^').Trim()
if($remote.object.sha -notin @($head,$parent)){throw 'Remote branch advanced independently'}
git -c credential.interactive=never -C $repo push --atomic origin main refs/tags/v1.0.27
if($LASTEXITCODE -ne 0){throw 'Source/tag push failed'}
'PUSHED main and v1.0.27'
$notes=[IO.File]::ReadAllText((Join-Path $out '发布说明.md'))
$release=$null
try{$release=GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/releases/tags/v1.0.27'}catch{if([int]$_.Exception.Response.StatusCode -ne 404){throw}}
if(-not $release){$release=GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/releases' 'POST' @{tag_name='v1.0.27';target_commitish=$head;name='LiteLaunch 1.0.27 · 更新与覆盖安装修复';body=$notes;draft=$true;prerelease=$false}}
$assets=@(
 @{name='LiteLaunch-Setup-1.0.27-x64.exe';path=$setup;type='application/octet-stream'},
 @{name='LiteLaunch-Plugin-BatchRenamer-1.0.0-x64.exe';path=(Join-Path $out 'LiteLaunch-Plugin-BatchRenamer-1.0.0-x64.exe');type='application/octet-stream'},
 @{name='LiteLaunch-Plugin-CursorSwitcher-1.0.1-x64.exe';path=(Join-Path $out 'LiteLaunch-Plugin-CursorSwitcher-1.0.1-x64.exe');type='application/octet-stream'},
 @{name='LiteLaunch-plugins.json';path=(Join-Path $out 'LiteLaunch-plugins.json');type='application/json'},
 @{name='LiteLaunch-source-v1.0.27.zip';path=(Join-Path $source 'LiteLaunch-source-v1.0.27.zip');type='application/zip'},
 @{name='SHA256SUMS.txt';path=(Join-Path $out 'SHA256SUMS.txt');type='text/plain'},
 @{name='THIRD-PARTY-NOTICES.txt';path=(Join-Path $out 'THIRD-PARTY-NOTICES.txt');type='text/plain'}
)
foreach($asset in $assets){
 $current=GitHubRequest ('/repos/ccl4-56-23-5/LiteLaunch/releases/'+$release.id)
 $found=@($current.assets|Where-Object name -ceq $asset.name)
 $hash=(Get-FileHash -LiteralPath $asset.path).Hash.ToLowerInvariant()
 if($found.Count){if($found.Count -ne 1 -or $found[0].digest -ine ('sha256:'+$hash)){throw ('Existing asset mismatch: '+$asset.name)}}
 else{
  $uri='https://uploads.github.com/repos/ccl4-56-23-5/LiteLaunch/releases/'+$release.id+'/assets?name='+[uri]::EscapeDataString($asset.name)
  $uploaded=Invoke-RestMethod -Uri $uri -Method POST -Headers $script:GitHubHeaders -InFile $asset.path -ContentType $asset.type -TimeoutSec 120
  if($uploaded.digest -ine ('sha256:'+$hash)){throw ('Uploaded asset digest mismatch: '+$asset.name)}
 }
 'UPLOADED '+$asset.name
}
$release=GitHubRequest ('/repos/ccl4-56-23-5/LiteLaunch/releases/'+$release.id) 'PATCH' @{name='LiteLaunch 1.0.27 · 更新与覆盖安装修复';body=$notes;draft=$false;prerelease=$false;make_latest='true'}
$release|ConvertTo-Json -Depth 20|Set-Content -LiteralPath (Join-Path $out 'github-release.json') -Encoding UTF8
@{version='1.0.27';state='published-awaiting-remote-verification';userTestConfirmed=$true;commit=$head;tag='v1.0.27';release=$release.html_url;installerSHA256=$accepted.installerSHA256;installerRebuiltAfterAcceptance=$false;remoteDownloadVerified=$false;cleanupAllowed=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $out 'publication-status.json') -Encoding UTF8
'PUBLISHED '+$release.html_url
