param([string]$Compiler = "$PSScriptRoot\..\..\..\tools\zig\zig-x86_64-windows-0.15.2\zig.exe",[string]$OutputDirectory = "$PSScriptRoot\..\..\..\temp\LiteLaunch-build-1.0.28\app",[switch]$ReusePluginPackages)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $output | Out-Null
$build = [IO.Path]::GetFullPath("$PSScriptRoot\..\..\..\temp\LiteLaunch-build-1.0.28")
New-Item -ItemType Directory -Force $build | Out-Null
if($ReusePluginPackages){
 $catalog=Get-Content -LiteralPath "$PSScriptRoot\plugins\catalog.json" -Raw|ConvertFrom-Json
 foreach($entry in $catalog.plugins){
  $package=Get-Item -LiteralPath "$PSScriptRoot\..\..\..\outputs\LiteLaunch\1.0.28\$($entry.asset)"
  if((Get-FileHash -LiteralPath $package.FullName).Hash -ine $entry.sha256 -or $package.VersionInfo.ProductName -ne $entry.productName -or $package.VersionInfo.FileVersion -ne ($entry.version+'.0')){throw ('Existing plugin identity mismatch: '+$entry.id)}
 }
}else{& "$PSScriptRoot\build-plugins.ps1" -Compiler $Compiler | Out-Null}
$resourceSource = Join-Path $build 'app-ever.rc'
$escapedAppResource = (Join-Path $PSScriptRoot 'app.rc').Replace('\','\\')
$escapedUpdater = (Join-Path $PSScriptRoot 'update\github-update.ps1').Replace('\','\\')
$escapedPlugins = (Join-Path $PSScriptRoot 'plugins\manager.ps1').Replace('\','\\')
$escapedCatalog = (Join-Path $PSScriptRoot 'plugins\catalog.json').Replace('\','\\')
@('// UTF8 resource paths.', '#pragma code_page(65001)', "#include `"$escapedAppResource`"", "302 RCDATA `"$escapedUpdater`"", "303 RCDATA `"$escapedPlugins`"", "304 RCDATA `"$escapedCatalog`"") | Set-Content -LiteralPath $resourceSource -Encoding UTF8
Push-Location $PSScriptRoot
try { & $Compiler rc /fo "$build\app.res" $resourceSource; if ($LASTEXITCODE -ne 0) { throw 'Resources failed' } } finally { Pop-Location }
& $Compiler cc -target x86_64-windows-gnu -std=c11 -Os -c -Wall -Wextra "$PSScriptRoot\main.c" -o "$build\main.o"
if ($LASTEXITCODE -ne 0) { throw 'C compilation failed' }
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -s -fno-exceptions -fno-rtti -municode '-Wl,--subsystem,windows' "$PSScriptRoot\glass.cpp" "$PSScriptRoot\startup.cpp" "$build\main.o" "$build\app.res" -o "$output\LiteLaunch.exe" -luser32 -lgdi32 -lgdiplus -lshlwapi -lshell32 -lole32 -lcomdlg32 -luuid -ladvapi32 -loleaut32 -lcomctl32 -limm32 -lpropsys -lversion
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
& "$PSScriptRoot\qa\assert-updater-embedded.ps1" -ApplicationExe "$output\LiteLaunch.exe"
Copy-Item -LiteralPath "$PSScriptRoot\README.md" -Destination "$output\使用说明.md" -Force
# Developer maintenance files are not installed.
Copy-Item -LiteralPath "$PSScriptRoot\安装使用说明.md" -Destination "$output\安装使用说明.md" -Force
New-Item -ItemType Directory -Force "$output\icons" | Out-Null
foreach($icon in @('logo.svg','pin.svg','gear.svg','close.svg','plus.svg','codex-official-legacy.png')) { Copy-Item -LiteralPath "$PSScriptRoot\icons\$icon" -Destination "$output\icons\$icon" -Force }
Copy-Item -LiteralPath "$PSScriptRoot\vendor\LICENSE.txt" -Destination "$output\NanoSVG-LICENSE.txt" -Force
Copy-Item -LiteralPath "$PSScriptRoot\官方图标来源.md" -Destination "$output\官方图标来源.md" -Force
Get-Item "$output\LiteLaunch.exe" | Select-Object FullName, Length

