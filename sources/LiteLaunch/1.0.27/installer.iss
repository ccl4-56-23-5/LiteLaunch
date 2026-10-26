[Setup]
AppId={{AA91BC95-36ED-4203-8942-CC6327501020}
AppName=轻启 LiteLaunch
AppVersion=1.0.27
VersionInfoVersion=1.0.27.0
VersionInfoProductVersion=1.0.27.0
VersionInfoProductName=LiteLaunch
VersionInfoDescription=LiteLaunch Setup
AppPublisher=LiteLaunch
DefaultDirName={code:GetInstallDir}
; Resolve legacy and running installations ourselves, including both registry views.
UsePreviousAppDir=no
UsePreviousTasks=yes
UsePreviousGroup=yes
DisableDirPage=no
DirExistsWarning=no
DefaultGroupName=轻启 LiteLaunch
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
WizardStyle=modern
SetupIconFile=app.ico
UninstallDisplayIcon={app}\LiteLaunch.exe
OutputDir=..\..\..\outputs\LiteLaunch\1.0.27
OutputBaseFilename=LiteLaunch-Setup-1.0.27-x64
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
SetupMutex=Local\LiteLaunch.Setup.v1
UninstallDisplayName=轻启 LiteLaunch

[Languages]
Name: chinese; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: desktopicon; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式："
Name: autostart; Description: "开机静默启动（不弹窗、不产生通知）"; GroupDescription: "启动选项："; Flags: unchecked

[Files]
Source: "..\..\..\temp\LiteLaunch-build-1.0.27\LiteLaunch-UpdateBridge.dll"; Flags: dontcopy
Source: "..\..\..\temp\LiteLaunch-installer\payload-1.0.27\LiteLaunch.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\..\temp\LiteLaunch-installer\payload-1.0.27\icons\*"; DestDir: "{app}\icons"; Flags: ignoreversion
Source: "..\..\..\temp\LiteLaunch-installer\payload-1.0.27\*.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\..\temp\LiteLaunch-installer\payload-1.0.27\NanoSVG-LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

Source: "..\..\..\temp\LiteLaunch-installer\payload-1.0.27\THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\轻启 LiteLaunch"; Filename: "{app}\LiteLaunch.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\轻启 LiteLaunch"; Filename: "{app}\LiteLaunch.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
; Optional verified plugins may sit beside the downloaded Setup. Remember the
; source directory so a first plugin click can install that exact catalog asset.
Root: HKCU; Subkey: "Software\LiteLaunch"; ValueType: string; ValueName: "PackageSource"; ValueData: "{src}"

[Run]
Filename: "{app}\LiteLaunch.exe"; Parameters: "--hidden"; Description: "启动轻启（中键唤醒）"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\LiteLaunch.exe"; Parameters: "--disable-startup"; Flags: runhidden waituntilterminated; RunOnceId: "DisableLiteLaunchStartup"

[Code]
#include "installer-detection.iss"
#include "installer-shutdown.iss"

procedure CurStepChanged(CurStep: TSetupStep);
var
  StartupParameter: String;
  ExitCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    if WizardIsTaskSelected('autostart') then
      StartupParameter := '--enable-startup'
    else
      StartupParameter := '--disable-startup';
    if not Exec(ExpandConstant('{app}\LiteLaunch.exe'), StartupParameter,
      ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ExitCode) then
      RaiseException('无法运行轻启的开机启动配置程序。');
    if ExitCode <> 0 then
      RaiseException('无法更新轻启的开机启动任务。请查看%LOCALAPPDATA%\LiteLaunch\startup.log。');
  end;
end;
