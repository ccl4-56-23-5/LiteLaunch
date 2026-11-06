#define wWinMain LiteLaunchUiEntry
#include "main.c"
#undef wWinMain
#include <assert.h>
static void pump(int ms){ULONGLONG end=GetTickCount64()+ms;MSG message;while(GetTickCount64()<end){while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(1);}}
int main(void){
    SetProcessDPIAware();glassInit();instance=GetModuleHandleW(NULL);pinned=TRUE;
    WNDCLASSW wc={0};wc.hInstance=instance;wc.lpfnWndProc=PanelProc;wc.lpszClassName=L"LiteLaunch.TimerFixture";assert(RegisterClassW(&wc));
    panel=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW,wc.lpszClassName,L"Timer test",WS_POPUP,100,100,288,316,NULL,NULL,instance,NULL);assert(panel);
    liveCapture=SetWindowDisplayAffinity(panel,0x11);assert(liveCapture);
    SetWindowPos(panel,HWND_TOPMOST,100,100,288,316,SWP_SHOWWINDOW|SWP_NOACTIVATE);pump(180);ULONGLONG first=lastCaptureTick;assert(first);
    pump(180);assert(lastCaptureTick>first);puts("PASS production SWP_SHOWWINDOW starts continuous background sampling without pointer movement");
    ShowWindow(panel,SW_HIDE);first=lastCaptureTick;pump(120);assert(lastCaptureTick==first);puts("PASS hidden panel stops background sampling");
    SetWindowPos(panel,HWND_TOPMOST,100,100,288,316,SWP_SHOWWINDOW|SWP_NOACTIVATE);pump(120);assert(lastCaptureTick>first);puts("PASS reopen restarts render timer");
    DestroyWindow(panel);glassShutdown();return 0;
}
