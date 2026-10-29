param([string]$Compiler)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$workspace=[IO.Path]::GetFullPath("$PSScriptRoot\..\..\..")
if(-not $Compiler){$Compiler=Join-Path $workspace 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe'}
$build=Join-Path $workspace 'temp\LiteLaunchBatchRenamer-build-1.0.2'
$out=Join-Path $workspace 'outputs\LiteLaunchBatchRenamer\1.0.2'
New-Item -ItemType Directory -Path $build,$out -Force|Out-Null
$scintilla=& "$PSScriptRoot\qa\build-scintilla.ps1" -Compiler $Compiler
& "$PSScriptRoot\qa\build-ever-regex.ps1" -OutputDirectory $build|Out-Null
$notices="SCINTILLA 5.6.6`r`n"+[IO.File]::ReadAllText("$PSScriptRoot\vendor\scintilla\License.txt")+"`r`nNANOSVG`r`n"+[IO.File]::ReadAllText("$PSScriptRoot\vendor\LICENSE.txt")
$notices+="`r`n"+[IO.File]::ReadAllText("$PSScriptRoot\THIRD-PARTY-NOTICES.txt")
[IO.File]::WriteAllText((Join-Path $build 'plugin-notices.txt'),$notices,(New-Object Text.UTF8Encoding($true)))
$icon=(Join-Path $PSScriptRoot 'plugins\renamer.ico').Replace('\','\\')
$worker=(Join-Path $build 'EverRegex.exe').Replace('\','\\')
$license=(Join-Path $build 'plugin-notices.txt').Replace('\','\\')
$manifest=(Join-Path $PSScriptRoot 'plugins\batch.manifest').Replace('\','\\')
$rc=@"
// UTF-8 plugin resources.
#pragma code_page(65001)
1 ICON "$icon"
1 24 "$manifest"
301 RCDATA "$worker"
400 RCDATA "$license"
1 VERSIONINFO
FILEVERSION 1,0,2,0
PRODUCTVERSION 1,0,2,0
FILEFLAGSMASK 0x3fL
FILEFLAGS 0
FILEOS 0x40004L
FILETYPE 1
FILESUBTYPE 0
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "080404b0"
  BEGIN
   VALUE "FileDescription", "LiteLaunch Batch Renamer Plugin\0"
   VALUE "FileVersion", "1.0.2.0\0"
   VALUE "ProductName", "LiteLaunch Batch Renamer\0"
   VALUE "ProductVersion", "1.0.2.0\0"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x0804, 1200
 END
END
"@
$rc|Set-Content -LiteralPath "$build\plugin.rc" -Encoding utf8
& $Compiler rc /fo "$build\plugin.res" "$build\plugin.rc"
if($LASTEXITCODE -ne 0){throw 'Resource build failed'}
foreach($module in @('rename','ever_rules','ever_regex')){
 & $Compiler c++ -DLITELAUNCH_BATCH_PLUGIN -target x86_64-windows-gnu -std=c++17 -Os -c "$PSScriptRoot\$module.cpp" -o "$build\$module.o"
 if($LASTEXITCODE -ne 0){throw "Module build failed: $module"}
}
$exe=Join-Path $out 'LiteLaunch-Plugin-BatchRenamer-1.0.2-x64.exe'
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -s -fno-exceptions -fno-rtti -municode '-Wl,--subsystem,windows' "$PSScriptRoot\plugins\ever-entry.cpp" "$PSScriptRoot\glass.cpp" "$build\rename.o" "$build\ever_rules.o" "$build\ever_regex.o" $scintilla.Archive "$build\plugin.res" -o $exe -luser32 -lgdi32 -lgdiplus -lshlwapi -lshell32 -lole32 -lcomdlg32 -luuid -ladvapi32 -loleaut32 -lcomctl32 -limm32 -lpropsys -lversion
if($LASTEXITCODE -ne 0){throw 'Plugin link failed'}
[ordered]@{id='batch-renamer';name='批量重命名';version='1.0.2';minHostVersion='1.0.24';asset=[IO.Path]::GetFileName($exe);productName='LiteLaunch Batch Renamer';sha256=(Get-FileHash -LiteralPath $exe).Hash;status='local-candidate'}|ConvertTo-Json|Set-Content -LiteralPath "$out\plugin-candidate.json" -Encoding utf8
Get-Item -LiteralPath $exe|Select-Object FullName,Length
