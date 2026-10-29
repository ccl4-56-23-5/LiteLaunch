/* Production PanelProc in a private process: no application startup, mutex,
   hooks, configuration reads/writes, plugin windows or real tray icon. */
#define LITELAUNCH_NO_RUNTIME_LOG
#define wWinMain LiteLaunchUnusedUiEntry
#include "../main.c"
#undef wWinMain
static BOOL legacyExit=FALSE;
static LRESULT CALLBACK FixtureProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(legacyExit&&msg==WM_QUERYENDSESSION)return DefWindowProcW(w,msg,wp,lp);
    if(legacyExit&&msg==WM_ENDSESSION)return 0;
    LRESULT result=PanelProc(w,msg,wp,lp);
    return legacyExit&&msg==WM_EXITAPP?0:result;
}
int wmain(int argc,WCHAR **argv){
    if(argc!=3)return 2;
    LPCWSTR mode=argv[1];instance=GetModuleHandleW(NULL);
    editing=!wcscmp(mode,L"busy");everPendingReady=!wcscmp(mode,L"pending");
    picking=!wcscmp(mode,L"picking");dragging=!wcscmp(mode,L"dragging");iconDragging=!wcscmp(mode,L"icon-dragging");
    legacyExit=!wcscmp(mode,L"legacy");
    WNDCLASSW wc={0};wc.hInstance=instance;wc.lpfnWndProc=FixtureProc;wc.lpszClassName=CLASSNAME;
    if(!RegisterClassW(&wc))return 3;
    if(wcscmp(mode,L"no-window")){
        panel=CreateWindowExW(WS_EX_TOOLWINDOW,CLASSNAME,L"LiteLaunch isolated shutdown QA",WS_POPUP,0,0,100,100,NULL,NULL,instance,NULL);
        if(!panel)return 4;
    }
    HANDLE ready=CreateFileW(argv[2],GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(ready==INVALID_HANDLE_VALUE)return 5;CloseHandle(ready);
    if(!wcscmp(mode,L"hung")||!wcscmp(mode,L"no-window")){Sleep(20000);return 0;}
    MSG message;while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    if(!wcscmp(mode,L"cleanup"))Sleep(600);
    return 0;
}
