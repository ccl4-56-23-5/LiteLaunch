param([string]$HostExe,[string]$ProtocolHostVersion='1.0.24')
$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
. "$source\plugins\manager.ps1" -LibraryOnly -HostVersion $ProtocolHostVersion
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$work=Join-Path $workspace ('temp\LiteLaunch-build-1.0.31\independent-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $work|Out-Null
$PluginRoot=Join-Path $work 'plugins'
if(-not $HostExe){$HostExe=Join-Path $workspace 'temp\LiteLaunch-installer\payload-1.0.31\LiteLaunch.exe'}
if(-not (Test-Path -LiteralPath $HostExe)){throw 'Build the host payload first, or pass -HostExe.'}
$before=(Get-FileHash -LiteralPath $HostExe).Hash
$catalog=Read-PluginCatalog (Get-Content "$source\plugins\catalog.json" -Raw|ConvertFrom-Json)
$next=$catalog.plugins|Where-Object id -eq 'cursor-switcher'
$old=$next|ConvertTo-Json|ConvertFrom-Json;$old.version='1.0.0';$old.asset='LiteLaunch-Plugin-CursorSwitcher-1.0.0-x64.exe';$old.releaseTag='plugin-cursor-v1.0.0'
# Compile a lower-version protocol fixture. It exercises activation and hash
# verification, without depending on an archived old release after cleanup.
$code=[IO.File]::ReadAllText("$source\plugins\cursor-switcher\CursorSwitcher.cs") -replace '(Assembly(?:File)?Version\(")[^"]+("\))','${1}1.0.0.0${2}'
$generated=Join-Path $work 'CursorPrevious.cs';[IO.File]::WriteAllText($generated,$code,[Text.UTF8Encoding]::new($true))
$oldFile=Join-Path $work $old.asset
& 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe' /nologo /target:winexe /platform:x64 /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll "/out:$oldFile" $generated "$source\plugins\cursor-switcher\CursorUi.cs"
if($LASTEXITCODE -ne 0){throw 'Previous-version protocol fixture failed to compile'}
$old.sha256=(Get-FileHash -LiteralPath $oldFile).Hash;Install-PluginBinary $oldFile $old
if((Get-InstalledPlugin $old).Record.version -ne '1.0.0'){throw 'Initial fixture installation failed'}
$oldCatalog=$catalog|ConvertTo-Json -Depth 8|ConvertFrom-Json;$oldCatalog.plugins=@($oldCatalog.plugins|Where-Object id -ne 'cursor-switcher')+@($old)
$merged=Merge-PluginCatalog $oldCatalog ([pscustomobject]@{schemaVersion=1;plugins=@($next)})
if($merged.plugins.Count -ne 2){throw 'Independent update erased another plugin'}
$installed=Resolve-PluginForLaunch $next @((Join-Path $workspace 'outputs\LiteLaunch\1.0.31'))
if($installed.Record.version -ne '1.0.1' -or (Get-Item -LiteralPath $installed.Path).VersionInfo.FileVersion -ne '1.0.1.0'){throw 'Verified cursor update was not activated'}
if((Get-FileHash -LiteralPath $HostExe).Hash -ne $before){throw 'Host changed during plugin update'}
@{ProtocolHostVersion=$ProtocolHostVersion;HostPayloadVersion=(Get-Item -LiteralPath $HostExe).VersionInfo.FileVersion;PluginBefore='1.0.0';PluginAfter='1.0.1';PreviousBinaryIsProtocolFixture=$true;HostUnchanged=$true;OtherCatalogPluginPreserved=$true;RealUserPluginInstallPerformed=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'verification.json') -Encoding utf8
'PASS lower-version fixture -> real cursor 1.0.1; protocol compatibility and host hash verified; no historical release directory required'