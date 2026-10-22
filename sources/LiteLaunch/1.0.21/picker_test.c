#define wWinMain LiteLaunchUiEntry
#include "main.c"
#undef wWinMain
int wmain(void){
    instance=GetModuleHandleW(NULL);dpi=96;WNDCLASSW wc={0};wc.hInstance=instance;wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"LiteLaunch.PickerFixture";wc.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);RegisterClassW(&wc);
    HWND fixture=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"Picker test",WS_POPUP,200,200,200,180,NULL,NULL,instance,NULL);ShowWindow(fixture,SW_SHOWNOACTIVATE);UpdateWindow(fixture);
    wc.lpszClassName=L"LiteLaunch.PickOverlay";wc.lpfnWndProc=PickerProc;RegisterClassW(&wc);
    pickOverlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"",WS_POPUP|WS_DISABLED,200,200,200,180,NULL,NULL,instance,NULL);
    BOOL alpha=SetLayeredWindowAttributes(pickOverlay,0,52,LWA_ALPHA);ShowWindow(pickOverlay,SW_SHOWNOACTIVATE);UpdateWindow(pickOverlay);POINT pt={250,250};HWND hit=WindowFromPoint(pt);
    BOOL pass=alpha&&hit==fixture;stopPickerVisual();DestroyWindow(fixture);printf("%s red overlay preserves underlying window hit-testing and cleanup\n",pass?"PASS":"FAIL");return pass?0:1;
}
