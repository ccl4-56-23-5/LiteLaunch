param([string]$Compiler = "$PSScriptRoot\..\..\..\tools\zig\zig-x86_64-windows-0.15.2\zig.exe")
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$payload = [IO.Path]::GetFullPath("$PSScriptRoot\..\..\..\temp\LiteLaunch-portable\1.0.21")
& "$PSScriptRoot\build.ps1" -Compiler $Compiler -OutputDirectory $payload
$built = Join-Path $payload 'LiteLaunch.exe'
if (-not (Test-Path -LiteralPath $built)) { throw 'Portable EXE was not built' }
$output = [IO.Path]::GetFullPath("$PSScriptRoot\..\..\..\outputs\LiteLaunch\1.0.21")
New-Item -ItemType Directory -Path $output -Force | Out-Null
$portable = Join-Path $output 'LiteLaunch.exe'
Copy-Item -LiteralPath $built -Destination $portable -Force
Get-Item -LiteralPath $portable | Select-Object FullName, Length
Get-FileHash -LiteralPath $portable -Algorithm SHA256 | Select-Object Hash
