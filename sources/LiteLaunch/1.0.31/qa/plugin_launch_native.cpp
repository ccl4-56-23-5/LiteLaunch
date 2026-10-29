// Exercise the production rename entry point with private state and an owned,
// off-screen window. Only this fixture's window is observed and closed.
#ifndef UNICODE
#define UNICODE
#endif
#define LITELAUNCH_EVER_TESTING
#define LITELAUNCH_BATCH_PLUGIN
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <fstream>
#include "../glass.h"
static std::wstring privateRoot;
static void CALLBACK ownWindowResult(HWND window,UINT,UINT_PTR id,DWORD){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId())return;
    KillTimer(window,id);
    std::ofstream file((privateRoot+L"\\result.json").c_str());
    file<<"{\"visible\":"<<(IsWindowVisible(window)?"true":"false")<<",\"rules\":"<<(GetDlgItem(window,2000)?"true":"false")<<",\"preview\":"<<(GetDlgItem(window,2001)?"true":"false")<<",\"editor\":"<<(GetDlgItem(window,400)?"true":"false")<<"}";file.close();PostMessageW(window,WM_CLOSE,0,0);
}
static HWND privateCreateWindow(DWORD ex,LPCWSTR cls,LPCWSTR title,DWORD style,int x,int y,int width,int height,HWND owner,HMENU menu,HINSTANCE instance,LPVOID data){
    bool main=(UINT_PTR)cls>65535&&!wcscmp(cls,L"LiteLaunch.Native.Ever.v1");
    HWND result=CreateWindowExW(ex,cls,title,style,main?-20000:x,main?-20000:y,width,height,owner,menu,instance,data);
    if(main&&result)SetTimer(result,0x515041,500,ownWindowResult);return result;
}
static HRESULT privateLocalFolder(HWND,int,HANDLE,DWORD,LPWSTR path){lstrcpynW(path,privateRoot.c_str(),MAX_PATH);return S_OK;}
static HANDLE privateMutex(LPSECURITY_ATTRIBUTES attributes,BOOL owner,LPCWSTR){std::wstring name=L"Local\\LiteLaunch.PluginLaunchQA."+std::to_wstring(GetCurrentProcessId());return CreateMutexW(attributes,owner,name.c_str());}
static BOOL privateActivate(HWND){return FALSE;}
#define CreateWindowExW privateCreateWindow
#define SHGetFolderPathW privateLocalFolder
#define SetForegroundWindow privateActivate
#include "../rename.cpp"
#undef CreateWindowExW
#define CreateMutexW privateMutex
#define wWinMain productionPluginMain
#include "../plugins/ever-entry.cpp"
#undef wWinMain
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR line,int show){
    wchar_t path[MAX_PATH]={};if(!GetEnvironmentVariableW(L"LL_PLUGIN_QA_ROOT",path,MAX_PATH))return 7;privateRoot=path;
    if(privateRoot.find(L"LL_PLUGIN_LAUNCH_QA_")==std::wstring::npos)return 8;
    std::wstring state=privateRoot+L"\\Rename";CreateDirectoryW(state.c_str(),nullptr);llrename::eTestHome=state+L"\\";
    return productionPluginMain(instance,previous,line,show);
}
