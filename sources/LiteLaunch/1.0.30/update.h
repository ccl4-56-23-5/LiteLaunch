#pragma once
/* The updater runs outside the panel so a slow network never blocks input. */
static void showGitHubUpdate(HWND owner){
    HRSRC resource=FindResourceW(NULL,MAKEINTRESOURCEW(302),RT_RCDATA);
    HGLOBAL loaded=resource?LoadResource(NULL,resource):NULL;
    const void *data=loaded?LockResource(loaded):NULL;DWORD size=resource?SizeofResource(NULL,resource):0;
    WCHAR root[MAX_PATH],folder[MAX_PATH],script[MAX_PATH],powershell[MAX_PATH],exe[MAX_PATH],command[4*MAX_PATH+180];
    if(!data||!size||FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,root)))goto failed;
    if(wcslen(root)>MAX_PATH-90)goto failed;
    lstrcatW(root,L"\\LiteLaunch");CreateDirectoryW(root,NULL);lstrcatW(root,L"\\Updates");CreateDirectoryW(root,NULL);
    swprintf(folder,MAX_PATH,L"%ls\\check-%lu-%llu",root,GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    if(!CreateDirectoryW(folder,NULL))goto failed;
    swprintf(script,MAX_PATH,L"%ls\\update.ps1",folder);
    {HANDLE file=CreateFileW(script,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);DWORD written=0;
     if(file==INVALID_HANDLE_VALUE)goto failed;BOOL ok=WriteFile(file,data,size,&written,NULL);CloseHandle(file);if(!ok||written!=size)goto failed;}
    if(!GetSystemDirectoryW(powershell,MAX_PATH)||!GetModuleFileNameW(NULL,exe,MAX_PATH))goto failed;
    lstrcatW(powershell,L"\\WindowsPowerShell\\v1.0\\powershell.exe");
    swprintf(command,sizeof(command)/sizeof(command[0]),L"\"%ls\" -NoProfile -STA -ExecutionPolicy Bypass -File \"%ls\" -ExecutablePath \"%ls\" -CurrentVersion 1.0.30 -OwnerPID %lu",powershell,script,exe,GetCurrentProcessId());
    /* Hide the console with CREATE_NO_WINDOW. SW_HIDE also hides the first
       WinForms dialog, so the repository/update prompt must start normally. */
    {STARTUPINFOW startup={0};PROCESS_INFORMATION process={0};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_SHOWNORMAL;
     if(!CreateProcessW(powershell,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,folder,&startup,&process))goto failed;
     CloseHandle(process.hThread);CloseHandle(process.hProcess);return;}
failed:
    MessageBoxW(owner,L"更新组件未能启动。请检查用户目录写入权限与Windows PowerShell。",L"LiteLaunch · GitHub更新",MB_ICONWARNING);
}
