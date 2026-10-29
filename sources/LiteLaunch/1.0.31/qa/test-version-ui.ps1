$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent;$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$work=Join-Path $workspace 'temp\LiteLaunch-build-1.0.31\version-ui-qa';New-Item -ItemType Directory -Force -Path $work|Out-Null
Copy-Item -LiteralPath "$source\update\ui.ps1" -Destination "$work\ui.ps1" -Force
Copy-Item -LiteralPath "$source\plugins\manager.ps1" -Destination "$work\manager.ps1" -Force
. "$work\manager.ps1" -LibraryOnly
. "$work\ui.ps1"
$script:checks=0
function Check($value,$name){if(-not $value){throw "FAIL $name"};$script:checks++;"PASS $name"}
$ui=Get-LiteLaunchUI
$view=New-LiteLaunchUpdateView '更新插件' '批量重命名'
Set-LiteLaunchUpdateState $view '1.0.4' '1.0.5' '发现新版本' '更新到 1.0.5' $true
Check ($view.Current.Text -eq '1.0.4' -and $view.Latest.Text -eq '1.0.5') 'current and available version numbers have separate persistent labels'
Check ($view.Primary.Text -eq '更新到 1.0.5' -and $view.Primary.Enabled) 'update button names the exact target version'
Check ($view.Form.BackColor.ToArgb() -eq $ui.Canvas.ToArgb() -and $view.Primary.Accent.ToArgb() -eq $ui.Accent.ToArgb()) 'update window uses the shared theme colors'
Check ($view.Primary -is [LiteLaunchUi.RoundButton] -and $view.Form.Font.Name -eq 'Microsoft YaHei UI') 'plugin and host updates share buttons and font'
Set-LiteLaunchUpdateState $view '1.0.5' '1.0.5' '已是最新版本' '已是最新版本' $false
Check ($view.Current.Text -eq '1.0.5' -and $view.Latest.Text -eq '1.0.5' -and -not $view.Primary.Enabled) 'up-to-date result still displays both versions'
Check ((Format-LiteLaunchVersion ([version]'1.0.31.0')) -eq '1.0.31' -and (Format-LiteLaunchVersion '1.0.0') -eq '1.0.0') 'host display preserves semantic version segments'
Set-LiteLaunchUpdateState $view '1.0.4' '1.0.5' '发现新版本，可直接更新此插件。' '更新到 1.0.5' $true
$view.Form.Show();[Windows.Forms.Application]::DoEvents()
$bitmap=New-Object Drawing.Bitmap($view.Form.Width,$view.Form.Height)
try{$view.Form.DrawToBitmap($bitmap,(New-Object Drawing.Rectangle(0,0,$bitmap.Width,$bitmap.Height)));$bitmap.Save((Join-Path $workspace 'outputs\LiteLaunch\1.0.31\update-ui-preview.png'))}finally{$bitmap.Dispose()}
$view.Form.Dispose()
# Exercise the production modal window with a private plan and a timer that only
# addresses this process's own form. No network, installation or external input.
$script:updated=0;$script:inspected=$false
function Get-PluginUpdateInfo([string]$Id){[pscustomobject]@{State='Available';CurrentVersion='1.0.4';LatestVersion='1.0.5';Plugin=[pscustomobject]@{id=$Id;name='批量重命名';version='1.0.5';minHostVersion='1.0.24'}}}
function Update-PluginFromGitHub([string]$Id,$Info){$script:updated++;[pscustomobject]@{State='Updated';Plugin=$Info.Plugin;Message='已更新到 1.0.5';Catalog=$null}}
$timer=New-Object Windows.Forms.Timer;$timer.Interval=100
$timer.Add_Tick({
 foreach($form in @([Windows.Forms.Application]::OpenForms)){
  $current=$form.Controls.Find('CurrentVersion',$true);$latest=$form.Controls.Find('LatestVersion',$true);$button=$form.Controls.Find('UpdateAction',$true)
  if($current.Count -ne 1 -or $latest.Count -ne 1 -or -not $button[0].Enabled){continue}
  $timer.Stop();Check ($current[0].Text -eq '1.0.4' -and $latest[0].Text -eq '1.0.5') 'production update dialog displays fetched versions before updating'
  Check ($script:updated -eq 0) 'checking a version does not silently start updating'
  $button[0].PerformClick();Check ($current[0].Text -eq '1.0.5' -and $latest[0].Text -eq '1.0.5') 'completed update keeps the resulting version visible'
  $script:inspected=$true;$form.Close();break
 }
})
try{$timer.Start();$result=Show-PluginUpdateWindow 'batch-renamer'}finally{$timer.Stop();$timer.Dispose()}
Check ($script:inspected -and $script:updated -eq 1 -and $result.State -eq 'Updated') 'one direct update action uses the version that was displayed'
"RESULT checks=$script:checks failures=0"
