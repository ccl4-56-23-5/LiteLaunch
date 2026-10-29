#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
static BOOL fixtureReady,fixtureIcon;
static int fixtureAdds,fixtureModifies;
static BOOL WINAPI fixtureNotify(DWORD action,PNOTIFYICONDATAW info){
    (void)info;
    if(action==NIM_ADD){fixtureAdds++;if(!fixtureReady||fixtureIcon)return FALSE;fixtureIcon=TRUE;return TRUE;}
    if(action==NIM_MODIFY){fixtureModifies++;return fixtureReady&&fixtureIcon;}
    if(action==NIM_DELETE){fixtureIcon=FALSE;return TRUE;}return FALSE;
}
#define Shell_NotifyIconW fixtureNotify
#ifndef LITELAUNCH_NO_RUNTIME_LOG
#define LITELAUNCH_NO_RUNTIME_LOG
#endif
#define wWinMain LiteLaunchUiEntry
#include "main.c"
#undef wWinMain
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int wmain(void){
    WNDCLASSW wc={0};wc.hInstance=GetModuleHandleW(NULL);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"LiteLaunch.TrayRetryFixture";
    CHECK(RegisterClassW(&wc));panel=CreateWindowW(wc.lpszClassName,L"",WS_POPUP,0,0,1,1,NULL,NULL,wc.hInstance,NULL);CHECK(panel);
    tray.cbSize=sizeof(tray);tray.hWnd=panel;tray.uID=1;
    CHECK(!ensureTray(FALSE));CHECK(!trayRegistered&&!fixtureIcon&&fixtureAdds==1);
    fixtureReady=TRUE;PanelProc(panel,WM_TIMER,TRAY_TIMER,0);CHECK(trayRegistered&&fixtureIcon);
    puts("PASS shell initially unavailable then timer retries registration");
    fixtureIcon=FALSE;taskbarCreated=RegisterWindowMessageW(L"LiteLaunch.TrayRetryFixture.TaskbarCreated");
    PanelProc(panel,taskbarCreated,0,0);CHECK(trayRegistered&&fixtureIcon);
    puts("PASS Explorer recreation registers tray again");
    fixtureIcon=FALSE;PanelProc(panel,WM_TIMER,TRAY_TIMER,0);CHECK(fixtureIcon&&trayRegistered&&fixtureModifies>0);
    puts("PASS missing tray without broadcast is repaired by periodic health check");
    PanelProc(panel,WM_DESTROY,0,0);CHECK(!trayRegistered&&!fixtureIcon);DestroyWindow(panel);
    puts("PASS shutdown removes tray and clears registration state");return 0;
}
