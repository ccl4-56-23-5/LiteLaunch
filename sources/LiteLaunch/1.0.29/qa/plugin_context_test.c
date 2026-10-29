#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>
static WCHAR root[MAX_PATH],command[4096];
static int dispatched=0,checks=0;
static DWORD flags=0;static WORD show=0;
static HRESULT privateFolder(HWND w,int f,HANDLE t,DWORD g,LPWSTR p){(void)w;(void)f;(void)t;(void)g;lstrcpyW(p,root);return S_OK;}
static BOOL captureStart(LPCWSTR a,LPWSTR c,LPSECURITY_ATTRIBUTES p,LPSECURITY_ATTRIBUTES t,BOOL inherit,DWORD f,LPVOID env,LPCWSTR dir,LPSTARTUPINFOW s,LPPROCESS_INFORMATION pi){
 (void)a;(void)p;(void)t;(void)inherit;(void)env;(void)dir;dispatched++;lstrcpynW(command,c,4096);flags=f;show=s->wShowWindow;pi->hThread=CreateEventW(NULL,FALSE,FALSE,NULL);pi->hProcess=CreateEventW(NULL,FALSE,FALSE,NULL);return TRUE;
}
#define SHGetFolderPathW privateFolder
#define CreateProcessW captureStart
#include "../plugins.h"
#define CHECK(x,n) do{if(!(x)){printf("FAIL %s line=%d\n",n,__LINE__);return 1;}checks++;printf("PASS %s\n",n);}while(0)
int wmain(int argc,WCHAR**argv){
 CHECK(argc==2,"private root provided");lstrcpynW(root,argv[1],MAX_PATH);CHECK(CreateDirectoryW(root,NULL),"fresh owned root");
 HMENU menu=pluginContextMenu(L"cursor-switcher");WCHAR label[100]={};GetMenuStringW(menu,PLUGIN_CONTEXT_UPDATE,label,100,MF_BYCOMMAND);
 CHECK(GetMenuItemCount(menu)==2&&wcsstr(label,L"GitHub"),"every occupied plugin offers direct GitHub update");DestroyMenu(menu);
 menu=pluginContextMenu(L"batch-renamer");CHECK(GetMenuItemCount(menu)==7&&GetMenuState(menu,PLUGIN_CONTEXT_UPDATE,MF_BYCOMMAND)!=(UINT)-1,"rename tools remain alongside update entry");DestroyMenu(menu);
 menu=pluginContextMenu(L"");CHECK(GetMenuItemCount(menu)==1&&GetMenuState(menu,PLUGIN_CONTEXT_UPDATE,MF_BYCOMMAND)==(UINT)-1,"empty slot only offers slot management");DestroyMenu(menu);
 pluginCheckUpdate(NULL,L"cursor-switcher",1,NULL);
 CHECK(dispatched==1&&wcsstr(command,L"-UpdateOnly")&&wcsstr(command,L"-RequestedPlugin \"cursor-switcher\""),"right-click dispatch targets exact plugin in update-only mode");
 CHECK(wcsstr(command,L"-HostVersion 1.0.29")&&wcsstr(command,L"-Slot 1"),"host version and slot survive dispatch");
 CHECK((flags&CREATE_NO_WINDOW)&&show==SW_SHOWNORMAL,"no console and visible update result window");
 WCHAR first[4096];lstrcpyW(first,command);Sleep(2);invokePlugin(NULL,L"batch-renamer",0,NULL,L"");
 CHECK(dispatched==2&&!wcsstr(command,L"-UpdateOnly")&&wcscmp(command,first),"normal launch remains separate from update mode");
 pluginCheckUpdate(NULL,L"",2,NULL);CHECK(dispatched==2,"empty slot cannot dispatch an update");
 printf("RESULT checks=%d failures=0\n",checks);return 0;
}
