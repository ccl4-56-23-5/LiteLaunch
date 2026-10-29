param([string]$Compiler)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$versionSource = Split-Path -Parent $PSScriptRoot
$workspaceDirectory = [IO.Path]::GetFullPath((Join-Path $versionSource '..\..\..'))
if (-not $Compiler) { $Compiler = Join-Path $workspaceDirectory 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe' }
$fixtureBuild = Join-Path $workspaceDirectory 'temp\LiteLaunch-build-1.0.30\ever-state-qa'
New-Item -ItemType Directory -Path $fixtureBuild -Force | Out-Null
$scintilla = & (Join-Path $PSScriptRoot 'build-scintilla.ps1') -Compiler $Compiler
$fixtureExe = Join-Path $fixtureBuild 'ever_form_test.exe'
$sourceNames = @('rename.cpp','rename.h','ever_window.inc','ever_rules.cpp','ever_rules.h','ever_regex.cpp','ever_regex.h','qa\ever_form_test.cpp','qa\run-ever-form.ps1')
$before = @($sourceNames | ForEach-Object { [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $versionSource $_) -Algorithm SHA256).Hash } })
& (Join-Path $PSScriptRoot 'build-ever-regex.ps1') -OutputDirectory $fixtureBuild | Out-Null
& $Compiler c++ -DEVER_REGEX_TESTING -target x86_64-windows-gnu -std=c++17 -Os -municode (Join-Path $PSScriptRoot 'ever_form_test.cpp') (Join-Path $versionSource 'ever_rules.cpp') (Join-Path $versionSource 'ever_regex.cpp') (Join-Path $versionSource 'glass.cpp') $scintilla.Archive -o $fixtureExe -luser32 -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -lshlwapi -limm32 -ladvapi32 -lpropsys -lversion -lgdiplus
if ($LASTEXITCODE -ne 0) { throw 'EVER private form compilation failed' }
$ownedRoot = Join-Path $fixtureBuild ('LL_EVER_FORM_QA_' + [Guid]::NewGuid().ToString('N'))
$log = Join-Path $fixtureBuild 'last-form-test.log'
& $fixtureExe $ownedRoot (Join-Path $fixtureBuild 'EverRegex.exe') 2>&1 | Tee-Object -FilePath $log | Write-Host
$testExitCode = $LASTEXITCODE
$after = @($sourceNames | ForEach-Object { [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $versionSource $_) -Algorithm SHA256).Hash } })
$stable = ($before | ConvertTo-Json -Depth 3 -Compress) -eq ($after | ConvertTo-Json -Depth 3 -Compress)
[ordered]@{ verifiedUtc = [DateTime]::UtcNow.ToString('o'); exitCode = $testExitCode; sourceStableDuringBuildAndTest = $stable; ownedRoot = $ownedRoot; testExeSha256 = (Get-FileHash -LiteralPath $fixtureExe -Algorithm SHA256).Hash; source = $before } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $fixtureBuild 'form-validation-manifest.json') -Encoding UTF8
if ($testExitCode -ne 0) { throw "EVER private form tests failed; see $log" }
if (-not $stable) { throw 'Production source changed during form test; rerun completed source' }
