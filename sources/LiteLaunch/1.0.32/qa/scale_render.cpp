/* Offline production-renderer previews. No launcher process or user profile is
   opened. Run scale_render.exe <output-directory>; inspect PNGs at native size. */
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <cmath>
#include <cstdio>
#include <string>
#include "../glass.h"

int wmain(int argc,wchar_t **argv){
    if(argc!=2)return 2;
    glassInit();glassTheme(2);glassRows(16,0);glassInteraction(-100,-100,-1,-1);
    const wchar_t *names[GLASS_SLOT_CAPACITY]={L"批量重命名",L"鼠标指针",L"添加插件",L"添加插件",
        L"控制面板",L"记事本",L"资源管理器",L"计算器",L"Codex",L"Microsoft Edge",L"远程桌面连接",L"科研项目目录",L"Windows Terminal",L"PDFgear",L"Zotero",L"添加应用"};
    void *icons[GLASS_SLOT_CAPACITY]={};BOOL occupied[GLASS_SLOT_CAPACITY]={};
    for(int i=0;i<16;i++)occupied[i]=i!=2&&i!=3&&i!=15;
    wchar_t system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);
    std::wstring executable=std::wstring(system)+L"\\notepad.exe";
    for(int i=4;i<15;i++)icons[i]=glassLoadIcon(executable.c_str(),names[i],executable.c_str(),144);
    const float scales[]={.5f,.6237f,.75f,.875f,1.f,1.25f,1.5f};
    const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};
    int failures=0;
    for(float scale:scales){
        int width=(int)std::lround(448*scale),height=(int)std::lround(524*scale);
        HDC screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);
        BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
        info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void *pixels=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(!bitmap){failures++;DeleteDC(memory);ReleaseDC(nullptr,screen);continue;}
        auto previous=SelectObject(memory,bitmap);
        glassPaint(memory,width,height,scale*96,-1,-1,FALSE,names,icons,occupied);
        GdiFlush();glassMaskPixels(pixels,width,height,scale*96);
        wchar_t filename[100];swprintf(filename,100,L"\\panel-%05.2f-percent.png",scale*100);
        std::wstring path=std::wstring(argv[1])+filename;
        {Gdiplus::Bitmap image(width,height,width*4,PixelFormat32bppPARGB,(BYTE*)pixels);
         if(image.Save(path.c_str(),&png,nullptr)!=Gdiplus::Ok)failures++;}
        wprintf(L"%ls (%dx%d)\n",path.c_str(),width,height);
        SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(nullptr,screen);
    }
    for(auto icon:icons)if(icon)glassFreeIcon(icon);
    glassShutdown();return failures?1:0;
}
