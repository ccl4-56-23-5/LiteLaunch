param([string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$project = Split-Path -Parent $PSScriptRoot
$workspace = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $project))
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $workspace 'temp\LiteLaunchBatchRenamer-build-1.0.2' }
$compiler = 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Windows .NET Framework C# compiler is unavailable.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = Join-Path $OutputDirectory 'EverRegex.exe'
& "$PSScriptRoot\build-pascal.ps1" -OutputDirectory $OutputDirectory|Out-Null
$pascal=Join-Path $OutputDirectory 'PascalRules.exe'
$exif=Join-Path $project 'vendor\exiftool-13.59_64.zip'
& $compiler /nologo /target:exe /platform:x64 /optimize+ /debug- /r:System.Web.Extensions.dll /r:System.Numerics.dll /r:System.Windows.Forms.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll "/resource:$pascal,LiteLaunch.Pascal" "/resource:$exif,LiteLaunch.ExifTool" "/out:$output" (Join-Path $project 'ever_regex.cs') (Join-Path $project 'renamer_rules.cs') (Join-Path $project 'renamer_metadata.cs') (Join-Path $project 'renamer_formats.cs') (Join-Path $project 'renamer_pascal.cs')
if ($LASTEXITCODE -ne 0) { throw 'EverRegex compilation failed.' }
Get-Item -LiteralPath $output | Select-Object FullName,Length
