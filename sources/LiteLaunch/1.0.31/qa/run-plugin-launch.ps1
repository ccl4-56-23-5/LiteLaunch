$ErrorActionPreference='Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$build=Join-Path $workspace 'temp\LiteLaunch-build-1.0.31\launch-qa';New-Item -ItemType Directory -Path $build -Force|Out-Null
$compiler=Join-Path $workspace 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe'
$nativePowerShell='C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
Add-Type -Path "$PSScriptRoot\launch-visibility-helper.cs"
foreach($mode in @(0,1)){
 $file=Join-Path $build ('visibility-'+$mode+'.txt')
 [NativePluginStartQA]::Run($nativePowerShell,"$PSScriptRoot\launch-visibility-child.ps1",$file,[short]$mode)
 $visible=(Get-Content -LiteralPath $file -Raw).Trim()
 if($visible -ne $(if($mode -eq 0){'False'}else{'True'})){throw 'Native startup visibility regression not reproduced'}
 'PASS native CREATE_NO_WINDOW startup '+$mode+' produces WinForms visible='+$visible
}
$scintilla=& "$PSScriptRoot\build-scintilla.ps1" -Compiler $compiler
$native=Join-Path $build 'LiteLaunch-Plugin-BatchRenamer-1.0.0-x64.exe'
& $compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -municode '-Wl,--subsystem,windows' "$PSScriptRoot\plugin_launch_native.cpp" "$source\ever_rules.cpp" "$source\ever_regex.cpp" "$source\glass.cpp" $scintilla.Archive "$workspace\temp\LiteLaunch-build-1.0.31\ever-plugin.res" -o $native -luser32 -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -lshlwapi -limm32 -ladvapi32 -lpropsys -lversion -lgdiplus
if($LASTEXITCODE -ne 0){throw 'Private production plugin entry fixture failed to compile'}
$catalog=Get-Content -LiteralPath "$source\plugins\catalog.json" -Raw|ConvertFrom-Json
$catalog.plugins=@($catalog.plugins|Where-Object id -eq 'batch-renamer');$catalog.plugins[0].sha256=(Get-FileHash -LiteralPath $native).Hash
$catalogPath=Join-Path $build 'private-catalog.json';$catalog|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $catalogPath -Encoding utf8BOM
$rc=@('// UTF8 resources','#pragma code_page(65001)')
foreach($pair in @(@(303,"$source\plugins\manager.ps1"),@(304,$catalogPath),@(110,"$source\plugins\renamer.ico"),@(111,"$source\plugins\cursor.ico"))){$rc+=($pair[0].ToString()+' RCDATA "'+$pair[1].Replace('\','\\')+'"')}
$rc|Set-Content -LiteralPath "$build\host.rc" -Encoding utf8NoBOM
& $compiler rc /fo "$build\host.res" "$build\host.rc";if($LASTEXITCODE -ne 0){throw 'Private host resources failed'}
& $compiler cc -target x86_64-windows-gnu -std=c11 -Os -municode '-Wl,--subsystem,windows' "$PSScriptRoot\plugin_launch_host.c" "$build\host.res" -o "$build\launch-host.exe" -luser32 -lshell32 -lole32 -luuid
if($LASTEXITCODE -ne 0){throw 'Private production host launch fixture failed to compile'}
$private=Join-Path $build ('LL_PLUGIN_LAUNCH_QA_'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $private|Out-Null
$oldLocal=$env:LOCALAPPDATA;$oldRoot=$env:LL_PLUGIN_QA_ROOT
try{
 $env:LOCALAPPDATA=$private;$env:LL_PLUGIN_QA_ROOT=$private
 foreach($scenario in @('first-click','installed-click')){
  $hostProcess=Start-Process -FilePath "$build\launch-host.exe" -PassThru -WindowStyle Hidden
  if(-not $hostProcess.WaitForExit(5000)){throw 'Private host failed to dispatch'}
  $result=Join-Path $private 'result.json';$deadline=[DateTime]::UtcNow.AddSeconds(20)
  while(-not (Test-Path -LiteralPath $result) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 100}
  if(-not (Test-Path -LiteralPath $result)){throw ('Native plugin did not create a window: '+$scenario)}
  $value=Get-Content -LiteralPath $result -Raw|ConvertFrom-Json
  if(-not $value.visible -or -not $value.rules -or -not $value.preview -or -not $value.editor){throw 'Native rename window or expected controls missing'}
  Move-Item -LiteralPath $result -Destination (Join-Path $private ($scenario+'.json'))
  'PASS '+$scenario+': production host -> Windows PowerShell 5.1 -> verified installation -> production native rename entry/window'
  if($scenario -eq 'first-click'){Move-Item -LiteralPath $native -Destination ($native+'.held');Start-Sleep -Milliseconds 300}
 }
}finally{$env:LOCALAPPDATA=$oldLocal;$env:LL_PLUGIN_QA_ROOT=$oldRoot;if(Test-Path -LiteralPath ($native+'.held')){Move-Item -LiteralPath ($native+'.held') -Destination $native}}
$missing=Join-Path $build ('LL_PLUGIN_MISSING_QA_'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $missing|Out-Null
$manager=[IO.File]::ReadAllText("$source\plugins\manager.ps1")
$manager=$manager.Replace('param([string]$CatalogPath,',('param([string]$Result,[string]$CatalogPath='''+$catalogPath+''',')).Replace('[string]$RequestedPlugin,','[string]$RequestedPlugin=''batch-renamer'',').Replace('Local\LiteLaunch.PluginManager.UI.','Local\LiteLaunch.PluginManager.PrivateQA.'+[guid]::NewGuid().ToString('N')+'.').Replace('Find-LocalPluginPackage $requested[0] (Get-LocalPluginDirectories)','Find-LocalPluginPackage $requested[0] @($PSScriptRoot)').Replace('[void][LiteLaunchPluginIni]::SetForegroundWindow($form.Handle)','[void]0')
$hook=@'
Add-Type 'using System;using System.Runtime.InteropServices; public static class PrivateFormVisible{[DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr w);}'
$form.StartPosition='Manual';$form.Location=New-Object Drawing.Point(-20000,-20000)
$qaTimer=New-Object Windows.Forms.Timer;$qaTimer.Interval=500;$qaTimer.Add_Tick({$qaTimer.Stop();@{visible=[PrivateFormVisible]::IsWindowVisible($form.Handle);selected=$list.SelectedItems.Count;name=$list.SelectedItems[0].Tag.id;message=$status.Text}|ConvertTo-Json|Set-Content -LiteralPath $Result -Encoding utf8;$form.Close()});$qaTimer.Start()
try{[void]$form.ShowDialog()}
'@
$manager=$manager.Replace('try{[void]$form.ShowDialog()}',$hook)
$missingScript=Join-Path $missing 'manager.ps1';[IO.File]::WriteAllText($missingScript,$manager,[Text.UTF8Encoding]::new($true))
try{$env:LOCALAPPDATA=$missing;[NativePluginStartQA]::Run($nativePowerShell,$missingScript,(Join-Path $missing 'result.json'),1)}finally{$env:LOCALAPPDATA=$oldLocal}
$missingResult=Get-Content -LiteralPath (Join-Path $missing 'result.json') -Raw|ConvertFrom-Json
if(-not $missingResult.visible -or $missingResult.selected -ne 1 -or $missingResult.name -ne 'batch-renamer'){throw 'Missing-plugin install manager must visibly select the requested plugin'}
'PASS missing package: visible Windows PowerShell 5.1 manager selects Batch Renamer and explains installation'
@{version='1.0.31';nativePowerShell='5.1';oldHiddenStartupReproduced=$true;normalStartupVisible=$true;firstClickInstallsVerifiedAdjacentPackageAndShowsNativeRename=$true;installedClickShowsNativeRename=$true;missingPluginManagerVisibleAndRequestedPluginSelected=$true;realUserConfigUsed=$false;realPluginInstallUsed=$false;fixture=$private}|ConvertTo-Json|Set-Content -LiteralPath "$workspace\outputs\LiteLaunch\1.0.31\launch-verification.json" -Encoding utf8
