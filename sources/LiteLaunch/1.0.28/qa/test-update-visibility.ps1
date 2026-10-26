$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$build=Join-Path $workspace 'temp\LiteLaunch-build-1.0.28\update-visibility'
New-Item -ItemType Directory -Force -Path $build|Out-Null
Add-Type -Path "$PSScriptRoot\launch-visibility-helper.cs"
foreach($mode in @(0,1)){
 $result=Join-Path $build ('visibility-'+$mode+'.txt')
 [NativePluginStartQA]::Run('C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe',"$PSScriptRoot\launch-visibility-child.ps1",$result,[int16]$mode)
 $visible=(Get-Content -LiteralPath $result -Raw).Trim()
 if($visible -ne $(if($mode -eq 0){'False'}else{'True'})){throw 'Native updater dialog visibility regression'}
 'PASS native PowerShell first dialog: startup='+$mode+' visible='+$visible
}
$launcher=[IO.File]::ReadAllText("$source\update.h")
if($launcher -notmatch 'startup.wShowWindow=SW_SHOWNORMAL' -or $launcher -notmatch 'CREATE_NO_WINDOW'){throw 'Production updater must show dialog while hiding console'}
'PASS production updater requests normal dialog startup'
