$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$build=Join-Path $workspace 'temp\LiteLaunch-build-1.0.28\cursor-qa';New-Item -ItemType Directory -Path $build -Force|Out-Null
$csc='C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$common=@('/nologo','/target:exe','/platform:x64','/r:System.Windows.Forms.dll','/r:System.Drawing.dll','/r:System.Web.Extensions.dll',"/win32icon:$source\plugins\cursor.ico")
foreach($test in @('CursorTests','CursorUiTests')){
 $exe=Join-Path $build ($test+'.exe');$files=@("$source\plugins\cursor-switcher\CursorSwitcher.cs","$source\plugins\cursor-switcher\CursorUi.cs","$PSScriptRoot\CursorTests.cs");if($test -ne 'CursorTests'){$files+="$PSScriptRoot\$test.cs"}
 & $csc @common "/main:$test" "/out:$exe" @files;if($LASTEXITCODE -ne 0){throw 'Cursor fixture compilation failed'}
 $private=Join-Path $build ('LL_CURSOR_QA_'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $private|Out-Null
 & $exe $private|Tee-Object -FilePath (Join-Path $build ($test+'.log'));if($LASTEXITCODE -ne 0){throw 'Cursor fixture failed'}
 if($test -eq 'CursorUiTests'){Copy-Item -LiteralPath (Join-Path $private 'cursor-preview.png') -Destination "$PSScriptRoot\cursor-preview.png" -Force}
}
