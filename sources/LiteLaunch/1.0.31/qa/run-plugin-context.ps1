$ErrorActionPreference='Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$build=Join-Path $workspace 'temp\LiteLaunch-build-1.0.31\plugin-context-qa';New-Item -ItemType Directory -Force -Path $build|Out-Null
$compiler=Join-Path $workspace 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe'
$rc=@('// Fixture resources','#pragma code_page(65001)')
foreach($pair in @(@(303,"$source\plugins\manager.ps1"),@(304,"$source\plugins\catalog.json"),@(305,"$source\update\ui.ps1"),@(110,"$source\plugins\renamer.ico"),@(111,"$source\plugins\cursor.ico"))){$rc+=($pair[0].ToString()+' RCDATA "'+$pair[1].Replace('\','\\')+'"')}
$rc|Set-Content -LiteralPath "$build\fixture.rc" -Encoding utf8
& $compiler rc /fo "$build\fixture.res" "$build\fixture.rc"
if($LASTEXITCODE -ne 0){throw 'Context fixture resources failed'}
& $compiler cc -target x86_64-windows-gnu -std=c11 -Os -municode "$PSScriptRoot\plugin_context_test.c" "$build\fixture.res" -o "$build\context-test.exe" -luser32 -lshell32 -lole32 -luuid
if($LASTEXITCODE -ne 0){throw 'Context fixture compilation failed'}
$owned=Join-Path $build ('LL_CONTEXT_QA_'+[guid]::NewGuid().ToString('N'))
& "$build\context-test.exe" $owned
if($LASTEXITCODE -ne 0){throw 'Context dispatch regression failed'}
