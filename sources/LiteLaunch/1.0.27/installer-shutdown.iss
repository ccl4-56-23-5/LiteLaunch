{ The 32-bit Setup engine loads a matching bridge even on x64 Windows. }
function LLProbe(Target: String): Integer;
  external 'LLProbe@files:LiteLaunch-UpdateBridge.dll cdecl setuponly';
function LLClose(Target: String; Timeout: Cardinal): Integer;
  external 'LLClose@files:LiteLaunch-UpdateBridge.dll cdecl setuponly';

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Target: String;
  Status: Integer;
begin
  Result := '';
  Target := AddBackslash(WizardDirValue) + 'LiteLaunch.exe';
  try
    Status := LLProbe(Target);
    if Status = 0 then Exit;
    if Status < 0 then
    begin
      Result := '无法核对正在运行的轻启。请从托盘退出旧程序后重试；安装尚未开始。';
      Exit;
    end;
    if WizardSilent then
    begin
      Result := '轻启正在运行，请使用可见安装向导确认退出，或先从托盘退出后重试。';
      Exit;
    end;
    if MsgBox('安装前需要正常退出正在运行的轻启。' + #13#10 +
      '请先完成或关闭应用编辑窗口。独立插件窗口不会被强制关闭。' + #13#10#13#10 +
      '是否现在退出轻启并继续安装？', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) <> IDYES then
    begin
      Result := '尚未退出轻启，安装尚未开始。准备好后可重试。';
      Exit;
    end;
    WizardForm.StatusLabel.Caption := '正在等待轻启正常退出…';
    Status := LLClose(Target, 8000);
    if Status <> 0 then
    begin
      if Status = -3 then
        Result := '轻启仍有编辑窗口或操作尚未完成。请关闭编辑窗口，或从托盘选择“退出轻启”，然后重试。'
      else if Status = -4 then
        Result := '轻启未在等待时间内退出。请完成当前操作并从托盘退出后重试。'
      else
        Result := '未能确认轻启已经退出。请从托盘退出旧程序后重试。';
      Result := Result + #13#10 + '尚未覆盖程序文件，也未强制结束任何进程。';
    end;
    Log('LiteLaunch cooperative shutdown result: ' + IntToStr(Status));
  except
    Result := '退出检查未完成：' + GetExceptionMessage + #13#10 +
      '请从托盘退出轻启后重试。安装尚未开始。';
  end;
end;
