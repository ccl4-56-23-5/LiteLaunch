$ErrorActionPreference='Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$source=Split-Path $PSScriptRoot -Parent;$workspace=$liteLaunchWorkspace
$build=Join-Path $workspace '.cache\LiteLaunch\1.1.0'
$compiler=$liteLaunchCompiler
& "$source\build-update-bridge.ps1" -Compiler $compiler
& $compiler cc -target x86_64-windows-gnu -std=c11 -Os -c "$PSScriptRoot\shutdown-fixture.c" -o "$build\shutdown-fixture.o"
if($LASTEXITCODE -ne 0){throw 'Shutdown fixture compilation failed'}
$fixture=Join-Path $build 'shutdown-fixture.exe'
& $compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -s -fno-exceptions -fno-rtti -municode "$source\glass.cpp" "$source\startup.cpp" "$build\shutdown-fixture.o" -o $fixture -luser32 -lgdi32 -lgdiplus -lshlwapi -lshell32 -lole32 -lcomdlg32 -luuid -ladvapi32 -loleaut32 -lcomctl32 -limm32 -lpropsys -lversion
if($LASTEXITCODE -ne 0){throw 'Shutdown fixture linking failed'}
$work=Join-Path $build ('shutdown-qa-'+[guid]::NewGuid().ToString('N'))
& 'C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe' -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\test-installer-shutdown.ps1" -Bridge "$build\LiteLaunch-UpdateBridge.dll" -Fixture $fixture -Work $work
if($LASTEXITCODE -ne 0){throw '32-bit installer bridge regression failed'}
Copy-Item -LiteralPath (Join-Path $work 'result.json') -Destination (Join-Path $workspace 'output\LiteLaunch\1.1.0\shutdown-verification.json') -Force
