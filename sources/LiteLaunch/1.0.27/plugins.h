#pragma once
static BOOL pluginPaths(WCHAR *root,WCHAR *ini){
    if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,root))||wcslen(root)>MAX_PATH-70)return FALSE;
    lstrcatW(root,L"\\LiteLaunch");CreateDirectoryW(root,NULL);swprintf(ini,MAX_PATH,L"%ls\\plugins.ini",root);return TRUE;
}
static void pluginSlot(int slot,WCHAR *id,WCHAR *label){
    WCHAR root[MAX_PATH],ini[MAX_PATH],key[24];id[0]=label[0]=0;if(!pluginPaths(root,ini))return;
    swprintf(key,24,L"Slot%d",slot);const WCHAR *defaultId=slot==0?L"batch-renamer":slot==1?L"cursor-switcher":L"";
    const WCHAR *defaultName=slot==0?L"批量重命名":slot==1?L"鼠标指针":L"添加插件";
    GetPrivateProfileStringW(L"Slots",key,defaultId,id,64,ini);GetPrivateProfileStringW(L"Labels",key,defaultName,label,128,ini);
    if(!id[0])lstrcpyW(label,L"添加插件");
    if(!wcscmp(id,L"ever-rename")){lstrcpyW(id,L"batch-renamer");lstrcpyW(label,L"批量重命名");}
    for(int i=0;id[i];i++)if(!((id[i]>=L'a'&&id[i]<=L'z')||(id[i]>=L'0'&&id[i]<=L'9')||id[i]==L'-')){id[0]=0;lstrcpyW(label,L"添加插件");break;}
}
static BOOL pluginWriteResource(int resourceId,LPCWSTR file){
    HRSRC resource=FindResourceW(NULL,MAKEINTRESOURCEW(resourceId),RT_RCDATA);if(!resource)return FALSE;
    DWORD size=SizeofResource(NULL,resource),written=0;const void *data=LockResource(LoadResource(NULL,resource));
    HANDLE handle=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(handle==INVALID_HANDLE_VALUE)return FALSE;
    BOOL ok=WriteFile(handle,data,size,&written,NULL)&&written==size;CloseHandle(handle);return ok;
}
static void invokePlugin(HWND owner,LPCWSTR id,int slot,HWND source,LPCWSTR action){
    WCHAR root[MAX_PATH],ini[MAX_PATH],folder[MAX_PATH],script[MAX_PATH],catalog[MAX_PATH],actionPath[MAX_PATH],exe[MAX_PATH],host[MAX_PATH],command[6*MAX_PATH+300];
    if(!pluginPaths(root,ini))goto failed;
    lstrcatW(root,L"\\PluginSessions");CreateDirectoryW(root,NULL);
    swprintf(folder,MAX_PATH,L"%ls\\%lu-%llu",root,GetCurrentProcessId(),(unsigned long long)GetTickCount64());if(!CreateDirectoryW(folder,NULL))goto failed;
    swprintf(script,MAX_PATH,L"%ls\\manager.ps1",folder);swprintf(catalog,MAX_PATH,L"%ls\\catalog.json",folder);swprintf(actionPath,MAX_PATH,L"%ls\\action.txt",folder);
    if(!pluginWriteResource(303,script)||!pluginWriteResource(304,catalog))goto failed;
    {WCHAR iconFile[MAX_PATH];swprintf(iconFile,MAX_PATH,L"%ls\\renamer.ico",folder);if(!pluginWriteResource(110,iconFile))goto failed;swprintf(iconFile,MAX_PATH,L"%ls\\cursor.ico",folder);if(!pluginWriteResource(111,iconFile))goto failed;}
    {HANDLE file=CreateFileW(actionPath,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);DWORD bytes=0;WORD bom=0xfeff;
     if(file==INVALID_HANDLE_VALUE)goto failed;BOOL ok=WriteFile(file,&bom,2,&bytes,NULL);DWORD size=(DWORD)(wcslen(action?action:L"")*sizeof(WCHAR));
     if(ok)ok=WriteFile(file,action?action:L"",size,&bytes,NULL)&&bytes==size;CloseHandle(file);if(!ok)goto failed;}
    if(!GetSystemDirectoryW(exe,MAX_PATH)||!GetModuleFileNameW(NULL,host,MAX_PATH))goto failed;lstrcatW(exe,L"\\WindowsPowerShell\\v1.0\\powershell.exe");
    swprintf(command,sizeof(command)/sizeof(command[0]),L"\"%ls\" -NoProfile -STA -ExecutionPolicy Bypass -File \"%ls\" -CatalogPath \"%ls\" -HostVersion 1.0.27 -HostExecutable \"%ls\" -RequestedPlugin \"%ls\" -Slot %d -SourceWindow %llu -ActionFile \"%ls\"",exe,script,catalog,host,id?id:L"",slot,(unsigned long long)(UINT_PTR)source,actionPath);
    /* CREATE_NO_WINDOW suppresses the PowerShell console. SW_HIDE also hides
       its first WinForms dialog, leaving an invisible manager holding the UI
       mutex. Interactive plugin windows must receive SW_SHOWNORMAL. */
    {STARTUPINFOW start={0};PROCESS_INFORMATION process={0};start.cb=sizeof(start);start.dwFlags=STARTF_USESHOWWINDOW;start.wShowWindow=SW_SHOWNORMAL;
     if(!CreateProcessW(exe,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,folder,&start,&process))goto failed;CloseHandle(process.hThread);CloseHandle(process.hProcess);return;}
failed:MessageBoxW(owner,L"无法打开插件管理，请检查用户目录和Windows PowerShell。",L"LiteLaunch插件",MB_ICONWARNING);
}
static void pluginRenameCommand(HWND owner,HWND source,int dpi,LPCWSTR action){(void)dpi;invokePlugin(owner,L"batch-renamer",-1,source,action);}
static HWND pluginRenameActiveWindow(void){return NULL;}
