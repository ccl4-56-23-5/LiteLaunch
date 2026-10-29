param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
if(-not $OutputDirectory){$OutputDirectory=Join-Path $workspace 'temp\LiteLaunchBatchRenamer-build-1.0.4'}
$root=Join-Path $workspace 'tools\fpc\3.2.2\{app}'
$compiler=Join-Path $root 'bin\i386-win32\fpc.exe'
if(-not (Test-Path -LiteralPath $compiler)){throw 'Free Pascal 3.2.2 Win32 compiler is required in tools/fpc/3.2.2/{app}.'}
$engine=Get-ChildItem -LiteralPath "$source\vendor\pascalscript-source" -Directory|Select-Object -First 1
$units=Join-Path $OutputDirectory 'pascal-units';New-Item -ItemType Directory -Force -Path $units|Out-Null
$args=@('-Mdelphi','-O2','-Xs','-CX','-XX',"-Fu$($engine.FullName)\Source","-FU$units","-FE$OutputDirectory",'-oPascalRules.exe')
foreach($directory in Get-ChildItem -LiteralPath "$root\units\i386-win32" -Directory){$args+="-Fu$($directory.FullName)"}
& $compiler @args "$source\pascal_worker.pas" | Write-Host
if($LASTEXITCODE -ne 0){throw 'Pascal Script worker compilation failed'}
Get-Item -LiteralPath "$OutputDirectory\PascalRules.exe"|Select-Object FullName,Length
