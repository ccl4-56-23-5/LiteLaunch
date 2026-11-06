$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$source = Split-Path -Parent $PSScriptRoot
$compiler = $liteLaunchCompiler
$build = Join-Path $liteLaunchBuildRoot 'scale-qa'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$object = Join-Path $build 'scale_test.o'
$exe = Join-Path $build 'scale_test.exe'
$libs = @('-luser32','-lgdi32','-lgdiplus','-lshlwapi','-lshell32','-lole32','-lcomdlg32','-luuid','-ladvapi32','-loleaut32','-lcomctl32','-limm32','-lpropsys','-lversion')
& $compiler cc -target x86_64-windows-gnu -std=c11 -Os -DLITELAUNCH_NO_RUNTIME_LOG -c -Wall -Wextra (Join-Path $source 'scale_test.c') -o $object
if ($LASTEXITCODE -ne 0) { throw 'Scale test C compilation failed' }
& $compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -s -fno-exceptions -fno-rtti -municode (Join-Path $source 'glass.cpp') (Join-Path $source 'startup.cpp') $object -o $exe @libs
if ($LASTEXITCODE -ne 0) { throw 'Scale test linking failed' }
$fixture = Join-Path $build ('profile-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
New-Item -ItemType Directory -Force -Path $liteLaunchOutputRoot | Out-Null
& $exe $fixture | Tee-Object -FilePath (Join-Path $liteLaunchOutputRoot 'scale-tests.log')
if ($LASTEXITCODE -ne 0) { throw 'Scale integration regression failed' }
