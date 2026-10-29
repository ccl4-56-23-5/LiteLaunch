$ErrorActionPreference='Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$compiler=Join-Path $workspace 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe'
$build=Join-Path $workspace 'temp\LiteLaunchBatchRenamer-build-1.0.1\batch-qa';New-Item -ItemType Directory -Path $build -Force|Out-Null
$scintilla=& "$PSScriptRoot\build-scintilla.ps1" -Compiler $compiler
& "$PSScriptRoot\build-ever-regex.ps1" -OutputDirectory $build|Out-Null
$exe=Join-Path $build 'batch_test.exe'
$icon=(Join-Path $source 'plugins\renamer.ico').Replace('\','\\');$manifest=(Join-Path $source 'plugins\batch.manifest').Replace('\','\\')
"// Preview resources`r`n#pragma code_page(65001)`r`n1 ICON `"$icon`"`r`n1 24 `"$manifest`""|Set-Content -LiteralPath (Join-Path $build 'preview.rc') -Encoding utf8NoBOM
& $compiler rc /fo "$build\preview.res" "$build\preview.rc"
if($LASTEXITCODE -ne 0){throw 'Preview resources failed'}
& $compiler c++ -DEVER_REGEX_TESTING -target x86_64-windows-gnu -std=c++17 -Os -municode "$PSScriptRoot\batch_test.cpp" "$source\ever_rules.cpp" "$source\ever_regex.cpp" "$source\glass.cpp" $scintilla.Archive "$build\preview.res" -o $exe -luser32 -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -lshlwapi -limm32 -ladvapi32 -lpropsys -lversion -lgdiplus
if($LASTEXITCODE -ne 0){throw 'Batch fixture compilation failed'}
$private=Join-Path $build ('LL_BATCH_QA_'+[guid]::NewGuid().ToString('N'))
& $exe $private (Join-Path $build 'EverRegex.exe')
if($LASTEXITCODE -ne 0){throw 'Batch pipeline regression failed'}
Copy-Item -LiteralPath (Join-Path $private 'preview.png') -Destination "$PSScriptRoot\batch-renamer-preview.png" -Force
