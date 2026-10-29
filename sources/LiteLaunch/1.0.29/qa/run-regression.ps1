$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$source = Split-Path -Parent $PSScriptRoot
$workspace = [IO.Path]::GetFullPath("$source\..\..\..")
$compiler = Join-Path $workspace 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe'
$build = Join-Path $workspace 'temp\LiteLaunch-build-1.0.29\regression'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$libs = @('-luser32','-lgdi32','-lgdiplus','-lshlwapi','-lshell32','-lole32','-lcomdlg32','-luuid','-ladvapi32','-loleaut32','-lcomctl32','-limm32','-lpropsys','-lversion')
foreach ($name in @('config_test','verify','tray_test','mouse_test')) {
    $object = Join-Path $build "$name.o"
    $exe = Join-Path $build "$name.exe"
    & $compiler cc -target x86_64-windows-gnu -std=c11 -Os -DLITELAUNCH_NO_RUNTIME_LOG -c -Wall -Wextra (Join-Path $source "$name.c") -o $object
    if ($LASTEXITCODE -ne 0) { throw "$name C compilation failed" }
    & $compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -s -fno-exceptions -fno-rtti -municode (Join-Path $source 'glass.cpp') (Join-Path $source 'startup.cpp') $object -o $exe @libs
    if ($LASTEXITCODE -ne 0) { throw "$name link failed" }
}
& (Join-Path $build 'config_test.exe') (Join-Path $source 'presets\LiteLaunch.ini')
if ($LASTEXITCODE -ne 0) { throw 'Config regression failed' }
$fixture = Join-Path $build 'verify-profile'
New-Item -ItemType Directory -Force -Path $fixture | Out-Null
& (Join-Path $build 'verify.exe') $fixture
if ($LASTEXITCODE -ne 0) { throw 'Functional regression failed' }
& (Join-Path $build 'tray_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Tray recovery regression failed' }

$mouseFixture = Join-Path $build 'mouse-profile'
New-Item -ItemType Directory -Force -Path $mouseFixture | Out-Null
& (Join-Path $build 'mouse_test.exe') $mouseFixture
if ($LASTEXITCODE -ne 0) { throw 'Mouse routing regression failed' }
