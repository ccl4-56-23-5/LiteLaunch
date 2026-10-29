param([string]$Compiler = "$PSScriptRoot\..\..\..\..\tools\zig\zig-x86_64-windows-0.15.2\zig.exe")
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$source = Split-Path -Parent $PSScriptRoot
$workspace = [IO.Path]::GetFullPath("$source\..\..\..")
$build = Join-Path $workspace 'temp\LiteLaunchBatchRenamer-build-1.0.5\rename-qa'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$exe = Join-Path $build 'rename_test.exe'
$snapshot = Join-Path $build 'rename-ui.bmp'
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -municode (Join-Path $source 'rename_test.cpp') -o $exe -luser32 -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -lshlwapi
if ($LASTEXITCODE -ne 0) { throw 'Rename test compilation failed' }
# The fixture creates a unique directory, uses only its own files, and closes its own UI.
& $exe --ui $snapshot
if ($LASTEXITCODE -ne 0) { throw 'Rename regression failed' }
