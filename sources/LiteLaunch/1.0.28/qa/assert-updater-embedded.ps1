param([Parameter(Mandatory=$true)][string]$ApplicationExe)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\package-resources.ps1"
$source=Split-Path $PSScriptRoot -Parent
$scriptPath=Join-Path $source 'update\github-update.ps1'
$bytes=Get-PackageResource $ApplicationExe 302
if((Get-BytesSHA256 $bytes) -ne (Get-FileHash -LiteralPath $scriptPath).Hash){throw 'Packaged updater differs from source; refusing to package a stale updater'}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($scriptPath,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Updater syntax invalid'}
$versionParameter=$ast.ParamBlock.Parameters|Where-Object {$_.Name.VariablePath.UserPath -eq 'CurrentVersion'}
$expected=$versionParameter.DefaultValue.SafeGetValue()
$file=Get-Item -LiteralPath $ApplicationExe
$actual=$file.VersionInfo.FileVersion.Trim()
if($actual.Split('.').Count -eq 3){$actual+='.0'}
if($expected.Split('.').Count -eq 3){$expected+='.0'}
if([version]$actual -ne [version]$expected){throw 'Packaged host and updater versions differ'}
if([Text.Encoding]::UTF8.GetString($bytes).Contains('发布中缺少唯一的LiteLaunch.exe')){throw 'Legacy portable updater found in installer payload'}
'PASS packaged resource 302 matches current installer updater and host version'
