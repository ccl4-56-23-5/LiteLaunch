param([string]$Compiler, [switch]$SkipRetry)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
if (-not $Compiler) { $Compiler = $liteLaunchCompiler }
& "$PSScriptRoot\..\test-startup.ps1" -Compiler $Compiler -SkipRetry:$SkipRetry
