param([string]$Compiler)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
if (-not $Compiler) { $Compiler = $liteLaunchCompiler }
$source = Split-Path -Parent $PSScriptRoot
$workspace = $liteLaunchWorkspace
$build = Join-Path $workspace '.cache\LiteLaunch\1.0.32\rename-qa'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$exe = Join-Path $build 'rename_test.exe'
$snapshot = Join-Path $build 'rename-ui.bmp'
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -municode (Join-Path $source 'rename_test.cpp') -o $exe -luser32 -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -lshlwapi
if ($LASTEXITCODE -ne 0) { throw 'Rename test compilation failed' }
# The fixture creates a unique directory, uses only its own files, and closes its own UI.
& $exe --ui $snapshot
if ($LASTEXITCODE -ne 0) { throw 'Rename regression failed' }
