$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$setup=Join-Path $workspace 'outputs\LiteLaunch\1.0.29\LiteLaunch-Setup-1.0.29-x64.exe'
$results=@()
foreach($scenario in 'available','latest'){
 $work=Join-Path $workspace ('temp\LiteLaunch-build-1.0.29\flow-'+$scenario+'-'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Force -Path $work|Out-Null
 $process=Start-Process -FilePath 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSScriptRoot+'\update-flow-child.ps1"'),'-Updater',('"'+$source+'\update\github-update.ps1"'),'-Setup',('"'+$setup+'"'),'-Work',('"'+$work+'"'),'-Scenario',$scenario) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $work 'child.log') -RedirectStandardError (Join-Path $work 'error.log')
 if(-not $process.WaitForExit(15000)){$process.Kill();throw 'Private update-flow fixture timed out'}
 if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $work 'result.json'))){Get-Content -LiteralPath (Join-Path $work 'error.log') -Tail 12;throw ('Private update flow failed: '+$scenario+'; see '+$work)}
 $results+=Get-Content -LiteralPath (Join-Path $work 'result.json') -Raw|ConvertFrom-Json
 'PASS '+$scenario+': production updater flow, private network and launch interception'
}
@{powerShell='Windows PowerShell 5.1';scenarios=$results;actualInstallerExecuted=$false;realUserSettingsUsed=$false}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $workspace 'outputs\LiteLaunch\1.0.29\update-flow-verification.json') -Encoding UTF8
