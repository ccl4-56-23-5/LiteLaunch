param([string]$Compiler)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
if (-not $Compiler) { $Compiler = $liteLaunchCompiler }
$build=[IO.Path]::GetFullPath($liteLaunchBuildRoot)
New-Item -ItemType Directory -Force -Path $build|Out-Null
& $Compiler cc -target x86-windows-gnu -std=c11 -Os -s -shared "$PSScriptRoot\installer\update-bridge.c" -o "$build\LiteLaunch-UpdateBridge.dll" -luser32
if($LASTEXITCODE -ne 0){throw 'Installer shutdown bridge compilation failed'}
