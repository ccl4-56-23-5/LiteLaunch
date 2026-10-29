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
$script:fixtureRelease=[pscustomobject]@{tag_name='v1.0.31';draft=$false;prerelease=$false;assets=@([pscustomobject]@{name='LiteLaunch-Setup-1.0.31-x64.exe';state='uploaded';size=$setupFile.Length;digest=('sha256:'+(Private-Hash $Setup));browser_download_url='https://github.com/ccl4-56-23-5/LiteLaunch/releases/download/v1.0.31/LiteLaunch-Setup-1.0.31-x64.exe'})}
function Invoke-RestMethod {param($Uri,$Headers,$TimeoutSec);$script:requestedEndpoint=[string]$Uri;return $script:fixtureRelease}
function Invoke-WebRequest {param([switch]$UseBasicParsing,$Uri,$OutFile,$TimeoutSec,$Headers);$script:downloadCount++;Copy-Item -LiteralPath $script:fixtureSetup -Destination $OutFile}
function Get-ScheduledTask {param($TaskName,$ErrorAction);return $null}
function Start-Process {
 param($FilePath,$ArgumentList,$WindowStyle)
 $script:launchCount++
 if([IO.Path]::GetFullPath($FilePath) -ne [IO.Path]::GetFullPath((Join-Path $Work 'LiteLaunch-Setup-1.0.31-x64.exe'))){throw 'Unexpected attempted installer launch'}
 if((Private-Hash $FilePath) -ne (Private-Hash $script:fixtureSetup)){throw 'Unverified installer launch'}
 $script:launchArguments=[string]$ArgumentList
 # Record the launch only. Never execute the installer in this test.
}
if((Get-Command Start-Process).CommandType -ne 'Function'){throw 'Installer launch interception missing'}
$copy=Join-Path $Work 'updater.ps1'
Copy-Item -LiteralPath $Updater -Destination $copy -Force
Copy-Item -LiteralPath (Join-Path (Split-Path $Updater -Parent) 'ui.ps1') -Destination (Join-Path $Work 'ui.ps1') -Force
Add-Type -AssemblyName System.Windows.Forms
$script:inspected=$false;$script:uiFailure=$null
$timer=New-Object Windows.Forms.Timer;$timer.Interval=100
$timer.Add_Tick({
 try{
  foreach($form in @([Windows.Forms.Application]::OpenForms)){
   $current=$form.Controls.Find('CurrentVersion',$true);$latest=$form.Controls.Find('LatestVersion',$true);$button=$form.Controls.Find('UpdateAction',$true)
   if($current.Count -ne 1 -or $latest.Count -ne 1 -or $latest[0].Text -eq '检测中…'){continue}
   $timer.Stop()
   if($current[0].Text -ne $installedVersion -or $latest[0].Text -ne '1.0.31'){throw 'Persistent current/latest version labels differ'}
   if($script:downloadCount -or $script:launchCount){throw 'Download began before explicit update action'}
   if($Scenario -eq 'available'){
    if($button[0].Text -ne '更新到 1.0.31' -or -not $button[0].Enabled){throw 'Versioned update action missing'}
    $button[0].PerformClick()
   }else{if($button[0].Enabled){throw 'Latest version incorrectly offers an update'};$form.Close()}
   $script:inspected=$true
  }
 }catch{$script:uiFailure=$_.Exception.Message;foreach($form in @([Windows.Forms.Application]::OpenForms)){$form.Close()}}
})
$owner=[Diagnostics.Process]::GetCurrentProcess()
$installedVersion=if($Scenario -eq 'available'){'1.0.27'}else{'1.0.31'}
$watch=[Diagnostics.Stopwatch]::StartNew()
try{$timer.Start();. $copy -ExecutablePath $owner.MainModule.FileName -OwnerPID $owner.Id -CurrentVersion $installedVersion}finally{$timer.Stop();$timer.Dispose()}
if($script:uiFailure){throw $script:uiFailure};if(-not $script:inspected){throw 'Version UI was not inspected'}
$watch.Stop()
if($script:requestedEndpoint -cne 'https://api.github.com/repos/ccl4-56-23-5/LiteLaunch/releases/latest'){throw 'Legacy configuration changed official endpoint'}
if((Private-Hash $legacy) -ne $legacyHash){throw 'Legacy user configuration modified'}
if($Scenario -eq 'available'){
 if($script:downloadCount -ne 1 -or $script:launchCount -ne 1){throw 'Update action did not download and open target installer'}
 if($script:launchArguments -match 'VERYSILENT|/SILENT|SUPPRESSMSGBOXES|FORCECLOSE'){throw 'Silent installer options unexpectedly introduced'}
}else{if($script:downloadCount -or $script:launchCount -or $watch.ElapsedMilliseconds -gt 5000){throw 'Latest-version window did not show its version without downloading'}}
@{scenario=$Scenario;officialEndpoint=$script:requestedEndpoint;downloads=$script:downloadCount;recordedInstallerOpens=$script:launchCount;actualInstallerExecuted=$false;legacySettingsUnchanged=$true;elapsedMs=$watch.ElapsedMilliseconds}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $Work 'result.json') -Encoding UTF8
'PASS '+$Scenario+': fixed repository and visible-version update flow'
