param([string]$Compiler="$PSScriptRoot\..\..\..\tools\zig\zig-x86_64-windows-0.15.2\zig.exe")
$ErrorActionPreference='Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$build=[IO.Path]::GetFullPath("$PSScriptRoot\..\..\..\temp\LiteLaunch-build-1.0.29")
New-Item -ItemType Directory -Force -Path $build|Out-Null
& $Compiler cc -target x86-windows-gnu -std=c11 -Os -s -shared "$PSScriptRoot\installer\update-bridge.c" -o "$build\LiteLaunch-UpdateBridge.dll" -luser32
if($LASTEXITCODE -ne 0){throw 'Installer shutdown bridge compilation failed'}
