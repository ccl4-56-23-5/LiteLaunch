{ Shared by the standalone download and the application update installer.
  Discovery is read-only. Never guess a fixed drive or overwrite user data. }
const
  LiteLaunchUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{AA91BC95-36ED-4203-8942-CC6327501020}_is1';

var
  InstallDirResolved: Boolean;
  ResolvedInstallDir: String;
  ExistingInstallDir: String;
  ExistingInstallVersion: String;
  InstallDiscoverySource: String;

function GetWindowThreadProcessId(Window: HWND; var ProcessId: Cardinal): Cardinal;
  external 'GetWindowThreadProcessId@user32.dll stdcall';
function OpenProcess(Access: Cardinal; Inherit: Boolean; ProcessId: Cardinal): THandle;
  external 'OpenProcess@kernel32.dll stdcall';
function QueryFullProcessImageName(Process: THandle; Flags: Cardinal;
  Filename: String; var Size: Cardinal): Boolean;
  external 'QueryFullProcessImageNameW@kernel32.dll stdcall';
function CloseHandle(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

function ExistingLiteLaunch(const Folder: String; var Version: String): Boolean;
var
  ExePath: String;
begin
  ExePath := AddBackslash(Folder) + 'LiteLaunch.exe';
  Result := (ExtractFileDrive(Folder) <> '') and
    (CompareText(RemoveBackslashUnlessRoot(Folder),
      AddBackslash(ExtractFileDrive(Folder))) <> 0) and
    FileExists(ExePath);
  if Result then
    Result := GetVersionNumbersString(ExePath, Version);
end;

function AcceptExistingDir(const Folder, Source: String): Boolean;
var
  Version, CleanFolder: String;
begin
  CleanFolder := RemoveBackslashUnlessRoot(Trim(RemoveQuotes(Folder)));
  Result := ExistingLiteLaunch(CleanFolder, Version);
  if Result then
  begin
    ResolvedInstallDir := CleanFolder;
    ExistingInstallDir := CleanFolder;
    ExistingInstallVersion := Version;
    InstallDiscoverySource := Source;
  end;
end;

function FindRunningInstall: Boolean;
var
  Window: HWND;
  ProcessId, Size: Cardinal;
  Process: THandle;
  ExePath: String;
begin
  Result := False;
  { The product's singleton window is local to this interactive desktop. }
  Window := FindWindowByClassName('LiteLaunch.Native.Panel.v1');
  if Window = 0 then Exit;
  ProcessId := 0;
  GetWindowThreadProcessId(Window, ProcessId);
  if ProcessId = 0 then Exit;
  Process := OpenProcess($1000, False, ProcessId); { QUERY_LIMITED_INFORMATION }
  if Process = 0 then Exit;
  try
    Size := 32768;
    SetLength(ExePath, Size);
    if QueryFullProcessImageName(Process, 0, ExePath, Size) then
    begin
      SetLength(ExePath, Size);
      if CompareText(ExtractFileName(ExePath), 'LiteLaunch.exe') = 0 then
        Result := AcceptExistingDir(ExtractFileDir(ExePath), 'running application');
    end;
  finally
    CloseHandle(Process);
  end;
end;

function FindRegisteredInstall(Root: Integer; const Source: String): Boolean;
var
  Folder: String;
begin
  Result := False;
  if RegQueryStringValue(Root, LiteLaunchUninstallKey, 'InstallLocation', Folder) then
    Result := AcceptExistingDir(Folder, Source);
  if (not Result) and RegQueryStringValue(Root, LiteLaunchUninstallKey,
    'Inno Setup: App Path', Folder) then
    Result := AcceptExistingDir(Folder, Source);
end;

function FindShortcutInstall(const LinkPath: String): Boolean;
var
  Shell, Link: Variant;
  ExePath: String;
begin
  Result := False;
  if not FileExists(LinkPath) then Exit;
  try
    Shell := CreateOleObject('WScript.Shell');
    Link := Shell.CreateShortcut(LinkPath);
    ExePath := Link.TargetPath;
    if CompareText(ExtractFileName(ExePath), 'LiteLaunch.exe') = 0 then
      Result := AcceptExistingDir(ExtractFileDir(ExePath), 'existing shortcut');
  except
    Log('Cannot read legacy shortcut: ' + GetExceptionMessage);
  end;
end;

function GetInstallDir(Param: String): String;
var
  Index: Integer;
  Argument: String;
  Found: Boolean;
begin
  if not InstallDirResolved then
  begin
    InstallDirResolved := True;
    ResolvedInstallDir := ExpandConstant('{localappdata}\Programs\LiteLaunch');
    { Explicit application-updater /DIR (or a deliberate manual override) wins. }
    for Index := ParamCount downto 1 do
    begin
      Argument := ParamStr(Index);
      if CompareText(Copy(Argument, 1, 5), '/DIR=') = 0 then
      begin
        ResolvedInstallDir := RemoveQuotes(Copy(Argument, 6, Length(Argument)));
        InstallDiscoverySource := 'explicit /DIR';
        AcceptExistingDir(ResolvedInstallDir, InstallDiscoverySource);
        Result := ResolvedInstallDir;
        Exit;
      end;
    end;
    Found := FindRunningInstall;
    if (not Found) and IsWin64 then Found := FindRegisteredInstall(HKCU64, 'HKCU64');
    if not Found then Found := FindRegisteredInstall(HKCU32, 'HKCU32');
    if (not Found) and IsWin64 then Found := FindRegisteredInstall(HKLM64, 'HKLM64');
    if not Found then Found := FindRegisteredInstall(HKLM32, 'HKLM32');
    if not Found then Found := FindShortcutInstall(ExpandConstant('{userdesktop}\轻启 LiteLaunch.lnk'));
    if not Found then Found := FindShortcutInstall(ExpandConstant('{userprograms}\轻启 LiteLaunch.lnk'));
    if not Found then Found := FindShortcutInstall(ExpandConstant('{userdesktop}\LiteLaunch.lnk'));
    if not Found then Found := FindShortcutInstall(ExpandConstant('{userprograms}\LiteLaunch.lnk'));
    if not Found then AcceptExistingDir(ResolvedInstallDir, 'default install directory');
    Log('LiteLaunch install discovery: ' + InstallDiscoverySource + ' -> ' + ResolvedInstallDir);
  end;
  Result := ResolvedInstallDir;
end;

procedure InitializeWizard;
begin
  WizardForm.DirEdit.Text := GetInstallDir('');
  if ExistingInstallDir <> '' then
  begin
    WizardForm.Caption := 'LiteLaunch · 覆盖更新';
    WizardForm.ReadyLabel.Caption := '已识别现有 LiteLaunch，将在原目录覆盖更新。请先保存正在编辑的重命名。';
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  { First installs still offer a directory; detected upgrades keep their location. }
  Result := (PageID = wpSelectDir) and (ExistingInstallDir <> '') and
    (CompareText(RemoveBackslashUnlessRoot(WizardDirValue), ExistingInstallDir) = 0);
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo,
  MemoTypeInfo, MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := '';
  if ExistingInstallDir <> '' then
    Result := '覆盖更新已安装的 LiteLaunch ' + ExistingInstallVersion + NewLine +
      Space + ExistingInstallDir + NewLine + NewLine;
  Result := Result + MemoDirInfo;
  if MemoGroupInfo <> '' then Result := Result + NewLine + NewLine + MemoGroupInfo;
  if MemoTasksInfo <> '' then Result := Result + NewLine + NewLine + MemoTasksInfo;
end;
