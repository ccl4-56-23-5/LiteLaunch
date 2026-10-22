param([string]$InnoCompiler="$PSScriptRoot\..\..\..\tools\inno\compiler\ISCC.exe",[string]$ApplicationExe)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$payload=[IO.Path]::GetFullPath("$PSScriptRoot\..\..\..\temp\LiteLaunch-installer\payload-1.0.21")
New-Item -ItemType Directory -Path $payload -Force|Out-Null
if($ApplicationExe){
 $ApplicationExe=[IO.Path]::GetFullPath($ApplicationExe)
 if((Get-Item -LiteralPath $ApplicationExe).VersionInfo.FileVersion -notin @('1.0.21','1.0.21.0')){throw 'Application version mismatch'}
 if($ApplicationExe -ine (Join-Path $payload 'LiteLaunch.exe')){Copy-Item -LiteralPath $ApplicationExe -Destination "$payload\LiteLaunch.exe" -Force}
 New-Item -ItemType Directory -Path "$payload\icons" -Force|Out-Null
 Get-ChildItem -LiteralPath "$PSScriptRoot\icons" -File|ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination "$payload\icons" -Force}
 foreach($name in 'README.md','开发文档.md'){Copy-Item -LiteralPath "$PSScriptRoot\$name" -Destination $payload -Force}
 Copy-Item -LiteralPath "$PSScriptRoot\vendor\LICENSE.txt" -Destination "$payload\NanoSVG-LICENSE.txt" -Force
}else{& "$PSScriptRoot\build.ps1" -OutputDirectory $payload}
$notices="SCINTILLA 5.6.6`r`n"+[IO.File]::ReadAllText("$PSScriptRoot\vendor\scintilla\License.txt")+"`r`nNANOSVG`r`n"+[IO.File]::ReadAllText("$PSScriptRoot\vendor\LICENSE.txt")
[IO.File]::WriteAllText("$payload\THIRD-PARTY-NOTICES.txt",$notices,(New-Object Text.UTF8Encoding($true)))
& $InnoCompiler "$PSScriptRoot\installer.iss"
if($LASTEXITCODE -ne 0){throw 'Installer compilation failed'}
Get-FileHash -LiteralPath "$PSScriptRoot\..\..\..\outputs\LiteLaunch\1.0.21\LiteLaunch-Setup-1.0.21-x64.exe"