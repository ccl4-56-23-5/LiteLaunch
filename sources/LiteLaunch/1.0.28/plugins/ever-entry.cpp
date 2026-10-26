#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <fstream>
#include <string>
#include "../rename.h"
#include "../glass.h"
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int){
    SetProcessDPIAware();HWND source=GetForegroundWindow();std::wstring action;int count=0;LPWSTR *args=CommandLineToArgvW(GetCommandLineW(),&count);
    for(int i=1;args&&i<count;i++){
        if(!wcscmp(args[i],L"--source-window")&&i+1<count)source=(HWND)(UINT_PTR)_wcstoui64(args[++i],nullptr,10);
        else if((!wcscmp(args[i],L"--rename-action"))&&i+1<count)action=args[++i];
        else if(!wcscmp(args[i],L"--action-file")&&i+1<count){
            std::ifstream file(args[++i],std::ios::binary);std::string data((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
            if(data.size()>16386||data.size()%2||data.size()<2||(unsigned char)data[0]!=255||(unsigned char)data[1]!=254){LocalFree(args);MessageBoxW(nullptr,L"插件调用参数无效。",L"EVER重命名",MB_ICONWARNING);return 2;}
            for(size_t n=2;n+1<data.size();n+=2)action.push_back((wchar_t)((unsigned char)data[n]|((unsigned char)data[n+1]<<8)));
        }
    }
    if(args)LocalFree(args);bool first=false;HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\LiteLaunch.EverPlugin.v1");first=mutex&&GetLastError()!=ERROR_ALREADY_EXISTS;
    if(!first){HWND existing=FindWindowW(L"LiteLaunch.Native.Ever.v1",nullptr);if(existing){HWND popup=GetLastActivePopup(existing);ShowWindow(existing,SW_RESTORE);SetForegroundWindow(popup?popup:existing);}if(mutex)CloseHandle(mutex);return existing?0:3;}
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX common={sizeof(common),ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&common);glassInit();
    WCHAR config[MAX_PATH];int scale=100;if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,config))){lstrcatW(config,L"\\LiteLaunch\\LiteLaunch.ini");glassTheme(GetPrivateProfileIntW(L"Panel",L"Theme",0,config));scale=GetPrivateProfileIntW(L"Panel",L"ScalePercent",100,config);if(scale<75||scale>200)scale=100;}
    HDC screen=GetDC(nullptr);int dpi=MulDiv(GetDeviceCaps(screen,LOGPIXELSX),scale,100);ReleaseDC(nullptr,screen);
    renameShowCommand(nullptr,IsWindow(source)?source:nullptr,dpi,action.c_str());glassShutdown();CoUninitialize();CloseHandle(mutex);return 0;
}
