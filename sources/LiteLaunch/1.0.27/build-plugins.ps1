param([string]$Compiler="$PSScriptRoot\..\..\..\tools\zig\zig-x86_64-windows-0.15.2\zig.exe",[ValidateSet('all','cursor-switcher','batch-renamer')][string]$Plugin='all')
$ErrorActionPreference='Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$workspace=[IO.Path]::GetFullPath("$PSScriptRoot\..\..\..")
$build=Join-Path $workspace 'temp\LiteLaunch-build-1.0.27'
$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.27'
New-Item -ItemType Directory -Path $build,$out -Force|Out-Null
$csc='C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$definitions=Get-Content -LiteralPath "$PSScriptRoot\plugins\releases.json" -Raw|ConvertFrom-Json
$cursorInfo=$definitions.plugins|Where-Object id -eq 'cursor-switcher';$everInfo=$definitions.plugins|Where-Object id -eq 'batch-renamer'
$cursor=Join-Path $out $cursorInfo.asset;$ever=Join-Path $out $everInfo.asset
if($Plugin -in @('all','cursor-switcher')){
 $cursorSource=[IO.File]::ReadAllText("$PSScriptRoot\plugins\cursor-switcher\CursorSwitcher.cs")
 $cursorSource=$cursorSource -replace '(Assembly(?:File)?Version\(")[^"]+("\))',('${1}'+$cursorInfo.version+'.0${2}')
 $generated=Join-Path $build 'CursorSwitcher.generated.cs';[IO.File]::WriteAllText($generated,$cursorSource,(New-Object Text.UTF8Encoding($true)))
 & $csc /nologo /target:winexe /platform:x64 /optimize+ /debug- /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll "/win32icon:$PSScriptRoot\plugins\cursor.ico" "/out:$cursor" $generated "$PSScriptRoot\plugins\cursor-switcher\CursorUi.cs"
 if($LASTEXITCODE -ne 0){throw 'Cursor plugin build failed'}
}
if($Plugin -in @('all','batch-renamer')){
$scintilla=& "$PSScriptRoot\qa\build-scintilla.ps1" -Compiler $Compiler
& "$PSScriptRoot\qa\build-ever-regex.ps1" -OutputDirectory $build|Out-Null
$notices="SCINTILLA 5.6.6`r`n"+[IO.File]::ReadAllText("$PSScriptRoot\vendor\scintilla\License.txt")+"`r`nNANOSVG`r`n"+[IO.File]::ReadAllText("$PSScriptRoot\vendor\LICENSE.txt")
[IO.File]::WriteAllText((Join-Path $build 'plugin-notices.txt'),$notices,(New-Object Text.UTF8Encoding($true)))
$icon=(Join-Path $PSScriptRoot 'plugins\renamer.ico').Replace('\','\\');$worker=(Join-Path $build 'EverRegex.exe').Replace('\','\\');$license=(Join-Path $build 'plugin-notices.txt').Replace('\','\\')
$everNumbers=$everInfo.version.Replace('.',',')+',0';$everVersion=$everInfo.version+'.0'
$manifest=(Join-Path $PSScriptRoot 'plugins\batch.manifest').Replace('\','\\')
$rc=@"
// UTF-8 plugin resources.
#pragma code_page(65001)
1 ICON "$icon"
1 24 "$manifest"
301 RCDATA "$worker"
400 RCDATA "$license"
1 VERSIONINFO
FILEVERSION $everNumbers
PRODUCTVERSION $everNumbers
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
   VALUE "FileVersion", "$everVersion\0"
   VALUE "ProductName", "LiteLaunch Batch Renamer\0"
   VALUE "ProductVersion", "$everVersion\0"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x0804, 1200
 END
END
"@
$rc|Set-Content -LiteralPath (Join-Path $build 'ever-plugin.rc') -Encoding utf8
& $Compiler rc /fo "$build\ever-plugin.res" "$build\ever-plugin.rc";if($LASTEXITCODE -ne 0){throw 'EVER plugin resources failed'}
foreach($module in @('rename','ever_rules','ever_regex')){
 & $Compiler c++ -DLITELAUNCH_BATCH_PLUGIN -target x86_64-windows-gnu -std=c++17 -Os -c "$PSScriptRoot\$module.cpp" -o "$build\$module.o"
 if($LASTEXITCODE -ne 0){throw ('EVER module failed: '+$module)}
}
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -s -fno-exceptions -fno-rtti -municode '-Wl,--subsystem,windows' "$PSScriptRoot\plugins\ever-entry.cpp" "$PSScriptRoot\glass.cpp" "$build\rename.o" "$build\ever_rules.o" "$build\ever_regex.o" $scintilla.Archive "$build\ever-plugin.res" -o $ever -luser32 -lgdi32 -lgdiplus -lshlwapi -lshell32 -lole32 -lcomdlg32 -luuid -ladvapi32 -loleaut32 -lcomctl32 -limm32 -lpropsys -lversion
if($LASTEXITCODE -ne 0){throw 'EVER plugin linking failed'}
}
$catalog=@{schemaVersion=1;plugins=@($definitions.plugins|ForEach-Object {
 $file=Join-Path $out $_.asset;if(-not (Test-Path -LiteralPath $file)){throw '未构建其他插件，请先运行build-plugins.ps1完成一次全部构建。'}
 $_|Add-Member -Force NoteProperty sha256 (Get-FileHash -LiteralPath $file).Hash;$_
})}
$catalog|ConvertTo-Json -Depth 8|Set-Content -LiteralPath "$PSScriptRoot\plugins\catalog.json" -Encoding utf8BOM
Copy-Item -LiteralPath "$PSScriptRoot\plugins\catalog.json" -Destination (Join-Path $out 'LiteLaunch-plugins.json') -Force
$catalog.plugins|Select-Object id,version,sha256
