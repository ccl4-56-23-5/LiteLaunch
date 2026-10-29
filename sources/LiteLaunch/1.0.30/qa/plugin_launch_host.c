#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>
static HRESULT privateLocalFolder(HWND window,int folder,HANDLE token,DWORD flags,LPWSTR path){
    (void)window;(void)folder;(void)token;(void)flags;
    return GetEnvironmentVariableW(L"LL_PLUGIN_QA_ROOT",path,MAX_PATH)>0?S_OK:E_FAIL;
}
#define SHGetFolderPathW privateLocalFolder
#include "../plugins.h"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR line,int show){
    (void)instance;(void)previous;(void)line;(void)show;
    invokePlugin(NULL,L"batch-renamer",0,NULL,L"");return 0;
}
