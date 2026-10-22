param([string]$InnoCompiler="$PSScriptRoot\..\..\..\tools\inno\compiler\ISCC.exe")
$ErrorActionPreference='Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$payload=[IO.Path]::GetFullPath("$PSScriptRoot\..\..\..\temp\LiteLaunch-installer\payload-1.0.21")
& "$PSScriptRoot\build.ps1" -OutputDirectory $payload
& $InnoCompiler "$PSScriptRoot\installer.iss"
if($LASTEXITCODE -ne 0){throw 'Installer compilation failed'}
