param([string]$InnoCompiler)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
if (-not $InnoCompiler) { $InnoCompiler = $liteLaunchInnoCompiler }
$source = Split-Path $PSScriptRoot -Parent
$production = Join-Path $source 'installer-detection.iss'
$sourceHash = (Get-FileHash -LiteralPath $production).Hash
$work = Join-Path $liteLaunchBuildRoot ('detection-qa-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $work,$liteLaunchOutputRoot | Out-Null
$resultPath = Join-Path $work 'checks.txt'
$code = [IO.File]::ReadAllText($production)
# Bind environment reads to in-memory fixtures; leave the detection logic intact.
$bindings = [ordered]@{
    FindWindowByClassName = 'FixtureFindWindowByClassName'
    RegQueryStringValue = 'FixtureRegQueryStringValue'
    FileExists = 'FixtureFileExists'
    GetVersionNumbersString = 'FixtureGetVersionNumbersString'
    ExpandConstant = 'FixtureExpandConstant'
    ParamCount = 'FixtureParamCount'
    ParamStr = 'FixtureParamStr'
    IsWin64 = 'FixtureWin64'
    WizardDirValue = 'FixtureWizardDir'
}
foreach ($name in $bindings.Keys) { $code = [regex]::Replace($code, ('\b' + $name + '\b'), $bindings[$name]) }
$fixture = @'
var
  FixtureWin64: Boolean;
  FixtureWizardDir, FixtureArgument, FixtureValidDir1, FixtureValidDir2: String;
  FixtureCU64, FixtureCU32, FixtureLM64, FixtureLM32, FixtureLegacyPath: String;
  FixtureVersionOk: Boolean;
  CheckCount, FailureCount: Integer;
  ReportText: String;

function FixtureFindWindowByClassName(Name: String): HWND;
begin
  Result := 0;
end;

function FixtureRegQueryStringValue(Root: Integer; Key, Name: String; var Value: String): Boolean;
begin
  Value := '';
  if Key <> 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{AA91BC95-36ED-4203-8942-CC6327501020}_is1' then
    RaiseException('Unexpected registry read');
  if Name = 'InstallLocation' then begin
    if Root = HKCU64 then Value := FixtureCU64;
    if Root = HKCU32 then Value := FixtureCU32;
    if Root = HKLM64 then Value := FixtureLM64;
    if Root = HKLM32 then Value := FixtureLM32;
  end else if (Name = 'Inno Setup: App Path') and (Root = HKCU64) then
    Value := FixtureLegacyPath;
  Result := Value <> '';
end;

function FixtureFileExists(Path: String): Boolean;
begin
  Result := ((FixtureValidDir1 <> '') and (CompareText(Path, AddBackslash(FixtureValidDir1) + 'LiteLaunch.exe') = 0)) or
    ((FixtureValidDir2 <> '') and (CompareText(Path, AddBackslash(FixtureValidDir2) + 'LiteLaunch.exe') = 0));
end;

function FixtureGetVersionNumbersString(Path: String; var Version: String): Boolean;
begin
  Result := FixtureVersionOk and FixtureFileExists(Path);
  if Result then Version := '1.0.31.0';
end;

function FixtureExpandConstant(Value: String): String;
begin
  Result := Value;
  StringChangeEx(Result, '{localappdata}', 'Q:\isolated-user', True);
  StringChangeEx(Result, '{userdesktop}', 'Q:\isolated-desktop', True);
  StringChangeEx(Result, '{userprograms}', 'Q:\isolated-programs', True);
end;

function FixtureParamCount: Integer;
begin
  if FixtureArgument = '' then Result := 0 else Result := 1;
end;

function FixtureParamStr(Index: Integer): String;
begin
  Result := FixtureArgument;
end;
'@
$tests = @'
procedure Check(Condition: Boolean; Name: String);
begin
  CheckCount := CheckCount + 1;
  if Condition then ReportText := ReportText + 'PASS ' + Name + #13#10
  else begin
    FailureCount := FailureCount + 1;
    ReportText := ReportText + 'FAIL ' + Name + #13#10;
  end;
end;

procedure ResetFixture;
begin
  InstallDirResolved := False; ResolvedInstallDir := ''; ExistingInstallDir := '';
  ExistingInstallVersion := ''; InstallDiscoverySource := '';
  FixtureWin64 := True; FixtureVersionOk := True; FixtureWizardDir := '';
  FixtureArgument := ''; FixtureValidDir1 := ''; FixtureValidDir2 := '';
  FixtureCU64 := ''; FixtureCU32 := ''; FixtureLM64 := ''; FixtureLM32 := ''; FixtureLegacyPath := '';
end;

function InitializeSetup: Boolean;
var
  Target, Version, Memo: String;
begin
  CheckCount := 0; FailureCount := 0; ReportText := '';
  try
    ResetFixture;
    Target := GetInstallDir('');
    Check(Target = 'Q:\isolated-user\Programs\LiteLaunch', 'first install uses current-user default');
    Check(ExistingInstallDir = '', 'first install does not invent an existing installation');
    FixtureWizardDir := Target;
    Check(not ShouldSkipPage(wpSelectDir), 'first install offers directory page');

    ResetFixture; FixtureArgument := '/DIR="Q:\chosen first install"';
    Check(GetInstallDir('') = 'Q:\chosen first install', 'first install accepts explicit non-default directory');
    FixtureWizardDir := 'Q:\chosen first install';
    Check(not ShouldSkipPage(wpSelectDir), 'explicit first install still offers directory page');

    ResetFixture; FixtureCU64 := 'Q:\custom installation'; FixtureValidDir1 := FixtureCU64;
    Check(GetInstallDir('') = FixtureCU64, 'HKCU64 non-default installation detected');
    Check((ExistingInstallVersion = '1.0.31.0') and (InstallDiscoverySource = 'HKCU64'), 'existing executable version and discovery source recorded');
    FixtureWizardDir := FixtureCU64;
    Check(ShouldSkipPage(wpSelectDir), 'unchanged non-default upgrade keeps directory');
    Check(not ShouldSkipPage(wpReady), 'upgrade does not skip unrelated pages');
    FixtureWizardDir := 'Q:\manual change';
    Check(not ShouldSkipPage(wpSelectDir), 'manual directory change remains visible');
    Memo := UpdateReadyMemo('  ', #13#10, '', 'directory details', '', '', '', 'startup settings');
    Check((Pos('1.0.31.0', Memo) > 0) and (Pos(FixtureCU64, Memo) > 0), 'upgrade ready text includes detected version and path');
    FixtureCU64 := 'Q:\changed registry';
    Check(GetInstallDir('') = 'Q:\custom installation', 'directory resolution is stable once cached');

    ResetFixture; FixtureCU32 := 'Q:\cu32'; FixtureValidDir1 := FixtureCU32;
    Check((GetInstallDir('') = FixtureCU32) and (InstallDiscoverySource = 'HKCU32'), 'HKCU32 legacy view detected');
    ResetFixture; FixtureLM64 := 'Q:\lm64'; FixtureValidDir1 := FixtureLM64;
    Check((GetInstallDir('') = FixtureLM64) and (InstallDiscoverySource = 'HKLM64'), 'HKLM64 installation detected');
    ResetFixture; FixtureLM32 := 'Q:\lm32'; FixtureValidDir1 := FixtureLM32;
    Check((GetInstallDir('') = FixtureLM32) and (InstallDiscoverySource = 'HKLM32'), 'HKLM32 legacy view detected');

    ResetFixture; FixtureCU64 := 'Q:\removed'; FixtureLegacyPath := 'Q:\legacy path'; FixtureValidDir1 := FixtureLegacyPath;
    Check(GetInstallDir('') = FixtureLegacyPath, 'invalid InstallLocation falls back to Inno App Path');
    ResetFixture; FixtureCU64 := 'Q:\per user'; FixtureLM64 := 'Q:\machine'; FixtureValidDir1 := FixtureCU64; FixtureValidDir2 := FixtureLM64;
    Check(GetInstallDir('') = FixtureCU64, 'current-user registration wins over machine registration');
    ResetFixture; FixtureCU64 := 'Q:\registered'; FixtureValidDir1 := FixtureCU64; FixtureArgument := '/DIR="Q:\explicit new"';
    Check(GetInstallDir('') = 'Q:\explicit new', 'explicit directory wins over registered installation');
    Check(ExistingInstallDir = '', 'explicit new directory is not misidentified as upgrade');
    ResetFixture; FixtureArgument := '/DIR="Q:\explicit old"'; FixtureValidDir1 := 'Q:\explicit old';
    Check((GetInstallDir('') = FixtureValidDir1) and (ExistingInstallDir = FixtureValidDir1), 'explicit existing directory is identified as upgrade');

    ResetFixture; FixtureCU64 := 'Q:\'; FixtureValidDir1 := 'Q:\';
    Check((GetInstallDir('') = 'Q:\isolated-user\Programs\LiteLaunch') and (ExistingInstallDir = ''), 'drive root registration rejected');
    ResetFixture; FixtureCU64 := 'relative folder'; FixtureValidDir1 := FixtureCU64;
    Check(ExistingLiteLaunch(FixtureCU64, Version) = False, 'relative registration rejected');
    ResetFixture; FixtureCU64 := 'Q:\not versioned'; FixtureValidDir1 := FixtureCU64; FixtureVersionOk := False;
    Check((GetInstallDir('') = 'Q:\isolated-user\Programs\LiteLaunch') and (ExistingInstallDir = ''), 'unversioned executable rejected');
    ResetFixture; FixtureValidDir1 := 'Q:\isolated-user\Programs\LiteLaunch';
    Check((GetInstallDir('') = FixtureValidDir1) and (InstallDiscoverySource = 'default install directory'), 'unregistered executable at default path detected');
    ResetFixture; FixtureCU32 := 'Q:\32-bit system'; FixtureValidDir1 := FixtureCU32; FixtureWin64 := False;
    Check((GetInstallDir('') = FixtureCU32) and (InstallDiscoverySource = 'HKCU32'), '32-bit view route skips unsupported 64-bit registry view');
  except
    FailureCount := FailureCount + 1;
    ReportText := ReportText + 'FAIL exception: ' + GetExceptionMessage + #13#10;
  end;
  ReportText := ReportText + Format('TOTAL %d FAILURES %d', [CheckCount, FailureCount]) + #13#10;
  SaveStringToFile('__RESULT_PATH__', ReportText, False);
  { This harness always exits before wizard creation or installation begins. }
  Result := False;
end;
'@
$tests = $tests.Replace('__RESULT_PATH__', $resultPath.Replace("'", "''"))
$setup = @'
[Setup]
AppId=LiteLaunch.InstallDetection.IsolatedQA
AppName=LiteLaunch install detection QA
AppVersion=1.1.0
DefaultDirName={tmp}\LiteLaunch-never-installed
CreateAppDir=no
Uninstallable=no
CreateUninstallRegKey=no
PrivilegesRequired=lowest
DisableStartupPrompt=yes
OutputBaseFilename=install-detection-test
Compression=none
[Code]
'@
$iss = Join-Path $work 'install-detection-test.iss'
[IO.File]::WriteAllText($iss, ($setup + "`r`n" + $fixture + "`r`n" + $code + "`r`n" + $tests), (New-Object Text.UTF8Encoding($true)))
& $InnoCompiler "/O$work" $iss *> (Join-Path $work 'compile.log')
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath (Join-Path $work 'compile.log') -Tail 15; throw 'Detection harness compilation failed' }
$process = Start-Process -FilePath (Join-Path $work 'install-detection-test.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-') -WindowStyle Hidden -Wait -PassThru
if (-not (Test-Path -LiteralPath $resultPath)) { throw "Detection harness produced no results (exit $($process.ExitCode))" }
$report = Get-Content -LiteralPath $resultPath -Raw
if ($report -notmatch 'TOTAL (\d+) FAILURES 0') { throw $report }
$count = [int]$Matches[1]
if ((Get-FileHash -LiteralPath $production).Hash -ne $sourceHash) { throw 'Production installer detection source changed during verification' }
$report | Set-Content -LiteralPath (Join-Path $liteLaunchOutputRoot 'installer-detection-tests.log') -Encoding utf8
[ordered]@{
    version = '1.1.0'; checks = $count; failures = 0; verifiedUtc = [datetime]::UtcNow.ToString('o')
    productionSourceSHA256 = $sourceHash; sourceLogicReused = $true; environmentReadsMocked = @($bindings.Keys)
    harnessExitCode = $process.ExitCode; initializeSetupReturnedFalse = $true
    realRegistryWritten = $false; applicationInstalled = $false; wizardShown = $false
    firstInstallDirectorySelectionCovered = $true; nonDefaultUpgradeDirectorySelectionCovered = $true
    notCovered = @('actual first installation','actual filesystem copying by Setup','running-application discovery','legacy shortcut resolution','manual UI acceptance')
    fixtureDirectory = $work
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $liteLaunchOutputRoot 'installer-detection-verification.json') -Encoding utf8
$report
