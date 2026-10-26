param([string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$project = Split-Path -Parent $PSScriptRoot
$workspace = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $project))
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $workspace 'temp\LiteLaunch-build-1.0.27' }
$compiler = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Windows .NET Framework C# compiler is unavailable.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = Join-Path $OutputDirectory 'EverRegex.exe'
& $compiler /nologo /target:exe /platform:x64 /optimize+ /debug- "/out:$output" (Join-Path $project 'ever_regex.cs')
if ($LASTEXITCODE -ne 0) { throw 'EverRegex compilation failed.' }
Get-Item -LiteralPath $output | Select-Object FullName,Length
