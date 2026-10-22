param([string]$Compiler = "$PSScriptRoot\..\..\..\..\tools\zig\zig-x86_64-windows-0.15.2\zig.exe", [switch]$SkipRetry)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
& "$PSScriptRoot\..\test-startup.ps1" -Compiler $Compiler -SkipRetry:$SkipRetry
