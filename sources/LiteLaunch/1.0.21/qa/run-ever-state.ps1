param([string]$Compiler)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$sourceDirectory = Split-Path -Parent $PSScriptRoot
$workspaceDirectory = [IO.Path]::GetFullPath((Join-Path $sourceDirectory '..\..\..'))
if (-not $Compiler) { $Compiler = Join-Path $workspaceDirectory 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe' }
$buildDirectory = Join-Path $workspaceDirectory 'temp\LiteLaunch-build-1.0.21\ever-state-qa'
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
$scintilla = & (Join-Path $PSScriptRoot 'build-scintilla.ps1') -Compiler $Compiler
$fixtureExe = Join-Path $buildDirectory 'ever_state_test.exe'
$fixtureSource = Join-Path $PSScriptRoot 'ever_state_test.cpp'
$logPath = Join-Path $buildDirectory 'last-test.log'
$sourceNames = @('rename.cpp','rename.h','ever_window.inc','ever_rules.cpp','ever_rules.h','ever_regex.cpp','ever_regex.h','vendor\scintilla\LiteLaunchEditor.h','qa\ever_state_test.cpp','qa\run-ever-state.ps1')
$inputHashes = @($sourceNames | ForEach-Object { [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $sourceDirectory $_) -Algorithm SHA256).Hash } })
& $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -municode $fixtureSource (Join-Path $sourceDirectory 'ever_rules.cpp') (Join-Path $sourceDirectory 'ever_regex.cpp') (Join-Path $sourceDirectory 'glass.cpp') $scintilla.Archive -o $fixtureExe -luser32 -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -lshlwapi -limm32 -ladvapi32 -lpropsys -lversion -lgdiplus
if ($LASTEXITCODE -ne 0) { throw 'EVER state fixture compilation failed.' }
$ownedRoot = Join-Path $buildDirectory ('LL_EVER_STATE_QA_' + [Guid]::NewGuid().ToString('N'))
$resolvedRoot = [IO.Path]::GetFullPath($ownedRoot)
if (-not $resolvedRoot.StartsWith($buildDirectory + '\', [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $resolvedRoot)) { throw 'QA isolation path validation failed.' }
Set-Content -LiteralPath (Join-Path $buildDirectory 'last-owned-root.txt') -Encoding UTF8 -Value $resolvedRoot
& $fixtureExe --suite $resolvedRoot 2>&1 | Tee-Object -FilePath $logPath | Write-Host
$testExitCode = $LASTEXITCODE
$outputHashes = @($sourceNames | ForEach-Object { [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $sourceDirectory $_) -Algorithm SHA256).Hash } })
$sourceStable = ($inputHashes | ConvertTo-Json -Depth 3 -Compress) -eq ($outputHashes | ConvertTo-Json -Depth 3 -Compress)
$summary = Get-Content -LiteralPath $logPath | Select-String -Pattern '^RESULT passed=(\d+) failed=(\d+)$' | Select-Object -Last 1
$passed = $null; $failed = $null
if ($summary) { $passed = [int]$summary.Matches[0].Groups[1].Value; $failed = [int]$summary.Matches[0].Groups[2].Value }
[ordered]@{ verifiedUtc = [DateTime]::UtcNow.ToString('o'); passed = $passed; failed = $failed; exitCode = $testExitCode; sourceStableDuringBuildAndTest = $sourceStable; ownedRoot = $resolvedRoot; testExeSha256 = (Get-FileHash -LiteralPath $fixtureExe -Algorithm SHA256).Hash; scintillaArchiveSha256 = (Get-FileHash -LiteralPath $scintilla.Archive -Algorithm SHA256).Hash; source = $inputHashes } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $buildDirectory 'state-validation-manifest.json') -Encoding UTF8
# Retain only this fixture's private tree for diagnosis; never delete user files.
[pscustomobject]@{ Fixture = $fixtureExe; OwnedRoot = $resolvedRoot; Log = $logPath; ExitCode = $testExitCode } | Format-List
if ($testExitCode -ne 0) { throw "EVER state/transaction tests failed; see $logPath" }
if (-not $sourceStable) { throw 'Production source changed during test; rerun against the completed source.' }
