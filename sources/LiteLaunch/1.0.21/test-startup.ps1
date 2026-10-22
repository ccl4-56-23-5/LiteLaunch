param([string]$Compiler = "$PSScriptRoot\..\..\..\tools\zig\zig-x86_64-windows-0.15.2\zig.exe", [switch]$SkipRetry)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$build = [IO.Path]::GetFullPath("$PSScriptRoot\..\..\..\temp\LiteLaunch-build-1.0.21")
New-Item -ItemType Directory -Force $build | Out-Null
$qa = Join-Path $build ('startup_qa_' + [Guid]::NewGuid().ToString('N') + '.exe')
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -s -fno-exceptions -fno-rtti -municode -Wall -Wextra -DLITELAUNCH_STARTUP_TESTING "$PSScriptRoot\startup.cpp" "$PSScriptRoot\startup_qa.cpp" -o $qa -lole32 -loleaut32 -luuid -ladvapi32 -lshell32
if ($LASTEXITCODE -ne 0) { throw 'Isolated startup QA compilation failed' }
$log = Join-Path $build $(if ($SkipRetry) { 'startup-qa-skip-retry.log' } else { 'startup-qa-last.log' })
$qaArguments = @()
if ($SkipRetry) { $qaArguments += '--skip-retry' }
& $qa @qaArguments | Tee-Object -FilePath $log
if ($LASTEXITCODE -ne 0) { throw 'Isolated startup QA failed' }
