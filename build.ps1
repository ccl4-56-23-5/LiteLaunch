param([string]$Compiler=(Join-Path $PSScriptRoot 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe'))
$ErrorActionPreference='Stop'
& "$PSScriptRoot\sources\LiteLaunch\1.0.21\build-portable.ps1" -Compiler $Compiler