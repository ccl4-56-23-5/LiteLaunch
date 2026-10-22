#define UNICODE
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <stdio.h>
#include "glass.h"
using namespace Gdiplus;
static COLORREF color=RGB(230,30,20);
static LRESULT CALLBACK BackProc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_PAINT){PAINTSTRUCT ps;HDC d=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);HBRUSH brush=CreateSolidBrush(color);FillRect(d,&r,brush);DeleteObject(brush);EndPaint(w,&ps);return 0;}return DefWindowProcW(w,m,a,b);}
int main(){
    SetProcessDPIAware();glassInit();WNDCLASSW wc={};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=BackProc;wc.lpszClassName=L"LiteLaunch.LiveFixture";RegisterClassW(&wc);
    HWND bg=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,wc.lpszClassName,L"Blur fixture",WS_POPUP,100,100,340,330,nullptr,nullptr,wc.hInstance,nullptr);ShowWindow(bg,SW_SHOWNOACTIVATE);UpdateWindow(bg);Sleep(100);
    HWND front=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TOOLWINDOW,wc.lpszClassName,L"Excluded renderer",WS_POPUP,110,110,288,275,nullptr,nullptr,wc.hInstance,nullptr);
    BOOL affinity=SetWindowDisplayAffinity(front,0x11);printf("capture exclusion=%d error=%lu\n",affinity,affinity?0:GetLastError());
    HDC screen=GetDC(nullptr),mem=CreateCompatibleDC(screen);BITMAPINFO bi={};bi.bmiHeader.biSize=40;bi.bmiHeader.biWidth=288;bi.bmiHeader.biHeight=-275;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void *pixels;HBITMAP bmp=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(mem,bmp);
    const WCHAR *names[12]={};void *icons[12]={};BOOL occupied[12]={};COLORREF samples[3];
    for(int i=0;i<3;i++){
        if(i==1){color=RGB(20,50,230);InvalidateRect(bg,nullptr,FALSE);UpdateWindow(bg);Sleep(100);}
        glassCapture(110,110,288,275);glassPaint(mem,288,275,96,-1,-1,FALSE,names,icons,occupied);GdiFlush();samples[i]=GetPixel(mem,140,25);
        unsigned char *p=(unsigned char*)pixels;for(int j=0;j<288*275;j++)p[j*4+3]=255;
        POINT dst={110,110},src={};SIZE size={288,275};BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};UpdateLayeredWindow(front,screen,&dst,&size,mem,&src,0,&blend,ULW_ALPHA);ShowWindow(front,SW_SHOWNOACTIVATE);Sleep(100);
    }
    printf("behind-red=%06lx behind-blue=%06lx repeat-blue=%06lx\n",samples[0],samples[1],samples[2]);
    bool passed=affinity&&GetRValue(samples[0])>GetBValue(samples[0])+70&&GetBValue(samples[1])>GetRValue(samples[1])+70&&samples[1]==samples[2];
    DestroyWindow(front);DestroyWindow(bg);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);ReleaseDC(nullptr,screen);glassShutdown();printf("LIVE BLUR %s\n",passed?"PASS":"FAIL");return passed?0:1;
}
