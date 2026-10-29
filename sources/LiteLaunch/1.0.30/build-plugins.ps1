param([string]$Compiler="$PSScriptRoot\..\..\..\tools\zig\zig-x86_64-windows-0.15.2\zig.exe",[ValidateSet('all','cursor-switcher','batch-renamer')][string]$Plugin='all')
$ErrorActionPreference='Stop'
. "$PSScriptRoot\qa\set-build-cache.ps1"
$workspace=[IO.Path]::GetFullPath("$PSScriptRoot\..\..\..")
$build=Join-Path $workspace 'temp\LiteLaunch-build-1.0.30'
$out=Join-Path $workspace 'outputs\LiteLaunch\1.0.30'
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
 $package=Join-Path $workspace ('outputs\LiteLaunchBatchRenamer\'+$everInfo.version+'\'+$everInfo.asset)
 $published=Get-Content -LiteralPath "$PSScriptRoot\plugins\catalog.json" -Raw|ConvertFrom-Json
 $entry=$published.plugins|Where-Object id -ceq 'batch-renamer'
 if(-not (Test-Path -LiteralPath $package) -or (Get-FileHash -LiteralPath $package).Hash -ine $entry.sha256){throw 'Build and verify the current independent Batch Renamer package first; host packaging reuses its released bytes.'}
 Copy-Item -LiteralPath $package -Destination $ever -Force
}
$catalog=@{schemaVersion=1;plugins=@($definitions.plugins|ForEach-Object {
 $file=Join-Path $out $_.asset;if(-not (Test-Path -LiteralPath $file)){throw '未构建其他插件，请先运行build-plugins.ps1完成一次全部构建。'}
 $_|Add-Member -Force NoteProperty sha256 (Get-FileHash -LiteralPath $file).Hash;$_
})}
$catalog|ConvertTo-Json -Depth 8|Set-Content -LiteralPath "$PSScriptRoot\plugins\catalog.json" -Encoding utf8BOM
Copy-Item -LiteralPath "$PSScriptRoot\plugins\catalog.json" -Destination (Join-Path $out 'LiteLaunch-plugins.json') -Force
$catalog.plugins|Select-Object id,version,sha256
