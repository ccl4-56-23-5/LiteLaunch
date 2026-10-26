$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.27'
$proof=Get-Content -LiteralPath (Join-Path $out 'github-verification.json') -Raw|ConvertFrom-Json
$accept=Get-Content -LiteralPath (Join-Path $out 'user-test-confirmation.json') -Raw|ConvertFrom-Json
if(-not $proof.cleanupAllowed -or -not $proof.pluginsDependOnlyOnVersion27 -or -not $accept.oldVersionsRemovalAuthorized){throw 'Old-release cleanup conditions not met'}
. "$PSScriptRoot\github-session.ps1"
$all=@(GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/releases?per_page=100')
$old=@($all|Where-Object tag_name -in @('v1.0.21','v1.0.22','v1.0.25'))
$record=@()
foreach($release in $old){
 [void](GitHubRequest ('/repos/ccl4-56-23-5/LiteLaunch/releases/'+$release.id) 'DELETE')
 $record+=@{tag=$release.tag_name;releaseId=$release.id;assetsRemoved=@($release.assets|ForEach-Object name)}
 'REMOVED old release '+$release.tag_name
}
foreach($tag in @('v1.0.21','v1.0.22','v1.0.25')){
 try{[void](GitHubRequest ('/repos/ccl4-56-23-5/LiteLaunch/git/refs/tags/'+$tag) 'DELETE')}catch{if([int]$_.Exception.Response.StatusCode -ne 404){throw}}
}
$remaining=@(GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/releases?per_page=100')
if($remaining.Count -ne 1 -or $remaining[0].tag_name -ne 'v1.0.27'){throw 'Unexpected remaining release set'}
if((GitHubRequest '/repos/ccl4-56-23-5/LiteLaunch/releases/latest').tag_name -ne 'v1.0.27'){throw 'Latest changed during cleanup'}
@{userAuthorized=$true;removed=$record;remainingReleases=@('v1.0.27');oldTagsRemoved=@('v1.0.21','v1.0.22','v1.0.25');gitCommitHistoryPreserved=$true;verifiedUtc=[datetime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $out 'github-old-release-cleanup.json') -Encoding UTF8
'PASS only v1.0.27 remains published'
