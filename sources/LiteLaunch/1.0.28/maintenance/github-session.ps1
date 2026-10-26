# Obtain an existing Git credential in memory only. Never print/store its value.
$ErrorActionPreference='Stop'
$env:GIT_TERMINAL_PROMPT='0'
$env:GCM_INTERACTIVE='never'
$credentialLines=@("protocol=https`nhost=github.com`n`n" | & git -c credential.interactive=never credential fill)
if($LASTEXITCODE -ne 0){throw 'Existing GitHub credential unavailable'}
$credentialValues=@{}
foreach($line in $credentialLines){$parts=$line -split '=',2;if($parts.Count -eq 2){$credentialValues[$parts[0]]=$parts[1]}}
if(-not $credentialValues.password){throw 'Existing GitHub credential has no access token'}
$script:GitHubHeaders=@{Authorization=('Bearer '+$credentialValues.password);'User-Agent'='LiteLaunch-Release-Maintenance';Accept='application/vnd.github+json';'X-GitHub-Api-Version'='2022-11-28'}
$credentialLines=$null;$credentialValues=$null
function GitHubRequest([string]$Path,[string]$Method='GET',$Body=$null){
 if($Path -notmatch '^/(repos/ccl4-56-23-5/LiteLaunch(?:/|$)|user$)'){throw 'Unexpected GitHub API path'}
 $arguments=@{Uri=('https://api.github.com'+$Path);Method=$Method;Headers=$script:GitHubHeaders;TimeoutSec=60}
 if($null -ne $Body){$arguments.Body=[Text.Encoding]::UTF8.GetBytes(($Body|ConvertTo-Json -Depth 30 -Compress));$arguments.ContentType='application/json; charset=utf-8'}
 $response=Invoke-RestMethod @arguments
 if($response -is [Array]){foreach($item in $response){Write-Output -NoEnumerate $item}}
 elseif($null -ne $response){Write-Output -NoEnumerate $response}
}
