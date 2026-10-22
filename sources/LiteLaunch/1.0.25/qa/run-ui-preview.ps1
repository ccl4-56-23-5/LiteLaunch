param([string]$Compiler)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$sourceDirectory = Split-Path -Parent $PSScriptRoot
$workspaceDirectory = [IO.Path]::GetFullPath((Join-Path $sourceDirectory '..\..\..'))
if (-not $Compiler) { $Compiler = Join-Path $workspaceDirectory 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe' }
$buildDirectory = Join-Path $workspaceDirectory 'temp\LiteLaunch-build-1.0.25\ui-preview'
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
$libs = @('-luser32','-lgdi32','-lgdiplus','-lshlwapi','-lshell32','-lole32','-lcomdlg32','-luuid','-ladvapi32','-loleaut32','-lcomctl32','-limm32','-lpropsys','-lversion')
$fixtureObject = Join-Path $buildDirectory 'ui_preview.o'
& $Compiler cc -target x86_64-windows-gnu -std=c11 -Os -c -DLITELAUNCH_NO_RUNTIME_LOG -Wall -Wextra (Join-Path $PSScriptRoot 'ui_preview.c') -o $fixtureObject
if ($LASTEXITCODE -ne 0) { throw 'Panel preview compilation failed' }
$fixtureExe = Join-Path $buildDirectory 'ui_preview.exe'
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -s -fno-exceptions -fno-rtti -municode (Join-Path $sourceDirectory 'glass.cpp') (Join-Path $sourceDirectory 'startup.cpp') $fixtureObject -o $fixtureExe @libs
if ($LASTEXITCODE -ne 0) { throw 'Panel preview link failed' }
$shots = Join-Path $buildDirectory 'shots'
New-Item -ItemType Directory -Path $shots -Force | Out-Null
# The preview loads chrome icons from an icons\ folder beside the executable.
Copy-Item -Path (Join-Path $sourceDirectory 'icons\*') -Destination (Join-Path $buildDirectory 'icons') -Force
& $fixtureExe $shots --dialogs
if ($LASTEXITCODE -ne 0) { throw 'Panel preview rendering failed' }
$rendered = @(Get-ChildItem -LiteralPath $shots -Filter '*.png')
if ($rendered.Count -lt 12) { throw "Expected at least 12 preview renders, found $($rendered.Count)" }
[ordered]@{ verifiedUtc = [DateTime]::UtcNow.ToString('o'); fixture = $fixtureExe; fixtureSha256 = (Get-FileHash -LiteralPath $fixtureExe -Algorithm SHA256).Hash; shots = $shots; renders = @($rendered.Name) } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $buildDirectory 'ui-preview-manifest.json') -Encoding UTF8
Get-ChildItem -LiteralPath $shots -Filter '*.png' | Select-Object Name, Length | Format-Table -AutoSize
