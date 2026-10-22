param([string]$Compiler = "$PSScriptRoot\..\..\..\tools\zig\zig-x86_64-windows-0.15.2\zig.exe",[string]$OutputDirectory = "$PSScriptRoot\..\..\..\temp\LiteLaunch-build-1.0.21\app")
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $output | Out-Null
$build = [IO.Path]::GetFullPath("$PSScriptRoot\..\..\..\temp\LiteLaunch-build-1.0.21")
New-Item -ItemType Directory -Force $build | Out-Null
$scintillaBuild = & "$PSScriptRoot\qa\build-scintilla.ps1" -Compiler $Compiler
& "$PSScriptRoot\qa\build-ever-regex.ps1" -OutputDirectory $build | Out-Null
$resourceSource = Join-Path $build 'app-ever.rc'
$escapedAppResource = (Join-Path $PSScriptRoot 'app.rc').Replace('\','\\')
$escapedWorker = (Join-Path $build 'EverRegex.exe').Replace('\','\\')
$escapedUpdater = (Join-Path $PSScriptRoot 'update\github-update.ps1').Replace('\','\\')
@('// UTF8 resource paths.', '#pragma code_page(65001)', "#include `"$escapedAppResource`"", "301 RCDATA `"$escapedWorker`"", "302 RCDATA `"$escapedUpdater`"") | Set-Content -LiteralPath $resourceSource -Encoding UTF8
Push-Location $PSScriptRoot
try { & $Compiler rc /fo "$build\app.res" $resourceSource; if ($LASTEXITCODE -ne 0) { throw 'Resources failed' } } finally { Pop-Location }
& $Compiler cc -target x86_64-windows-gnu -std=c11 -Os -c -Wall -Wextra "$PSScriptRoot\main.c" -o "$build\main.o"
if ($LASTEXITCODE -ne 0) { throw 'C compilation failed' }
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -c "$PSScriptRoot\rename.cpp" -o "$build\rename.o"
if ($LASTEXITCODE -ne 0) { throw 'Rename compilation failed' }
foreach($module in @('ever_rules','ever_regex')) {
    & $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -c "$PSScriptRoot\$module.cpp" -o "$build\$module.o"
    if ($LASTEXITCODE -ne 0) { throw "$module compilation failed" }
}
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -s -fno-exceptions -fno-rtti -municode '-Wl,--subsystem,windows' "$PSScriptRoot\glass.cpp" "$PSScriptRoot\startup.cpp" "$build\rename.o" "$build\ever_rules.o" "$build\ever_regex.o" $scintillaBuild.Archive "$build\main.o" "$build\app.res" -o "$output\LiteLaunch.exe" -luser32 -lgdi32 -lgdiplus -lshlwapi -lshell32 -lole32 -lcomdlg32 -luuid -ladvapi32 -loleaut32 -lcomctl32 -limm32 -lpropsys -lversion
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
Copy-Item -LiteralPath "$PSScriptRoot\README.md" -Destination "$output\使用说明.md" -Force
Copy-Item -LiteralPath "$PSScriptRoot\开发文档.md" -Destination "$output\开发文档.md" -Force
Copy-Item -LiteralPath "$PSScriptRoot\安装使用说明.md" -Destination "$output\安装使用说明.md" -Force
New-Item -ItemType Directory -Force "$output\icons" | Out-Null
foreach($icon in @('logo.svg','pin.svg','gear.svg','close.svg','plus.svg','codex-official-legacy.png')) { Copy-Item -LiteralPath "$PSScriptRoot\icons\$icon" -Destination "$output\icons\$icon" -Force }
Copy-Item -LiteralPath "$PSScriptRoot\vendor\LICENSE.txt" -Destination "$output\NanoSVG-LICENSE.txt" -Force
Copy-Item -LiteralPath "$PSScriptRoot\官方图标来源.md" -Destination "$output\官方图标来源.md" -Force
Get-Item "$output\LiteLaunch.exe" | Select-Object FullName, Length

