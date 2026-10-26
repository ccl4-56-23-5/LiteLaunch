param([Parameter(Mandatory=$true)][string]$Updater,[Parameter(Mandatory=$true)][string]$Setup,[Parameter(Mandatory=$true)][string]$Work,[ValidateSet('available','latest')][string]$Scenario)
$ErrorActionPreference='Stop'
function Private-Hash([string]$Path){$stream=[IO.File]::OpenRead($Path);$sha=[Security.Cryptography.SHA256]::Create();try{([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','')}finally{$stream.Dispose();$sha.Dispose()}}
New-Item -ItemType Directory -Force -Path $Work|Out-Null
$env:LOCALAPPDATA=Join-Path $Work 'profile'
$legacyFolder=Join-Path $env:LOCALAPPDATA 'LiteLaunch';New-Item -ItemType Directory -Force -Path $legacyFolder|Out-Null
$legacy=Join-Path $legacyFolder 'update-settings.json'
'{"RepositoryUrl":"https://github.com/untrusted/example"}'|Set-Content -LiteralPath $legacy -Encoding UTF8
$legacyHash=Private-Hash $legacy
$script:downloadCount=0;$script:launchCount=0;$script:requestedEndpoint='';$script:launchArguments=''
$setupFile=Get-Item -LiteralPath $Setup
$script:fixtureSetup=$setupFile.FullName
$script:fixtureRelease=[pscustomobject]@{tag_name='v1.0.28';draft=$false;prerelease=$false;assets=@([pscustomobject]@{name='LiteLaunch-Setup-1.0.28-x64.exe';state='uploaded';size=$setupFile.Length;digest=('sha256:'+(Private-Hash $Setup));browser_download_url='https://github.com/ccl4-56-23-5/LiteLaunch/releases/download/v1.0.28/LiteLaunch-Setup-1.0.28-x64.exe'})}
function Invoke-RestMethod {param($Uri,$Headers,$TimeoutSec);$script:requestedEndpoint=[string]$Uri;return $script:fixtureRelease}
function Invoke-WebRequest {param([switch]$UseBasicParsing,$Uri,$OutFile,$TimeoutSec,$Headers);$script:downloadCount++;Copy-Item -LiteralPath $script:fixtureSetup -Destination $OutFile}
function Get-ScheduledTask {param($TaskName,$ErrorAction);return $null}
function Start-Process {
 param($FilePath,$ArgumentList)
 $script:launchCount++
 if([IO.Path]::GetFullPath($FilePath) -ne [IO.Path]::GetFullPath((Join-Path $Work 'LiteLaunch-Setup-1.0.28-x64.exe'))){throw 'Unexpected attempted installer launch'}
 if((Private-Hash $FilePath) -ne (Private-Hash $script:fixtureSetup)){throw 'Unverified installer launch'}
 $script:launchArguments=[string]$ArgumentList
 # Record the launch only. Never execute the installer in this test.
}
if((Get-Command Start-Process).CommandType -ne 'Function'){throw 'Installer launch interception missing'}
$copy=Join-Path $Work 'updater.ps1'
$text=[IO.File]::ReadAllText($Updater)
$errorHandler="}catch{[Windows.Forms.MessageBox]::Show(`$_.Exception.Message,'LiteLaunch更新未完成','OK','Warning')|Out-Null}"
if(-not $text.Contains($errorHandler)){throw 'Private test error interception does not match production source'}
$text=$text.Replace($errorHandler,'}catch{throw}')
$text=$text.Replace("`$progress.StartPosition='CenterScreen'","`$progress.StartPosition='Manual';`$progress.Location=New-Object Drawing.Point(-20000,-20000)")
$text=$text.Replace("`$notice.StartPosition='CenterScreen'","`$notice.StartPosition='Manual';`$notice.Location=New-Object Drawing.Point(-20000,-20000)")
[IO.File]::WriteAllText($copy,$text,(New-Object Text.UTF8Encoding($true)))
$owner=[Diagnostics.Process]::GetCurrentProcess()
$installedVersion=if($Scenario -eq 'available'){'1.0.27'}else{'1.0.28'}
$watch=[Diagnostics.Stopwatch]::StartNew()
. $copy -ExecutablePath $owner.MainModule.FileName -OwnerPID $owner.Id -CurrentVersion $installedVersion
$watch.Stop()
if($script:requestedEndpoint -cne 'https://api.github.com/repos/ccl4-56-23-5/LiteLaunch/releases/latest'){throw 'Legacy configuration changed official endpoint'}
if((Private-Hash $legacy) -ne $legacyHash){throw 'Legacy user configuration modified'}
if($Scenario -eq 'available'){
 if($script:downloadCount -ne 1 -or $script:launchCount -ne 1){throw 'Available update did not download and open automatically'}
 if($script:launchArguments -match 'VERYSILENT|/SILENT|SUPPRESSMSGBOXES|FORCECLOSE'){throw 'Silent installer options unexpectedly introduced'}
}else{if($script:downloadCount -or $script:launchCount -or $watch.ElapsedMilliseconds -gt 5000){throw 'Latest-version notice did not close without interaction'}}
@{scenario=$Scenario;officialEndpoint=$script:requestedEndpoint;downloads=$script:downloadCount;recordedInstallerOpens=$script:launchCount;actualInstallerExecuted=$false;legacySettingsUnchanged=$true;elapsedMs=$watch.ElapsedMilliseconds}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $Work 'result.json') -Encoding UTF8
'PASS '+$Scenario+': fixed repository and no-interaction update flow'
