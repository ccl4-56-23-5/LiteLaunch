/* ui_preview.c -- offline renderer for the LiteLaunch panel.
 *
 * Renders glassPaint()/glassMaskPixels() straight into a DIB and writes PNGs,
 * so a visual change can be reviewed as pixels instead of by launching the app.
 * It links the production rendering path through main.c, exactly like verify.c,
 * and performs no file operations outside the fixture directory it is given.
 *
 * usage: ui_preview.exe <output-directory>
 */
#define wWinMain LiteLaunchUiEntry
#include "../main.c"
#undef wWinMain

#define PREVIEW_ROWS 4
#define PREVIEW_WIDTH 448
#define PREVIEW_HEIGHT (GLASS_GRID_TOP+PREVIEW_ROWS*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT)

static const WCHAR *previewNames[CELL_COUNT];
static void *previewIcons[CELL_COUNT];
static BOOL previewOccupied[CELL_COUNT];

static void fillFixture(void){
    static const WCHAR *appNames[]={L"记事本",L"画图",L"计算器",L"资源管理器",L"命令提示符",L"远程桌面连接",L"Codex",L"Windows 终端"};
    static const WCHAR *appPaths[]={L"notepad.exe",L"mspaint.exe",L"calc.exe",L"explorer.exe",L"cmd.exe",L"mstsc.exe",L"",L"wt.exe"};
    previewNames[0]=L"EVER重命名";previewNames[1]=L"控制面板";previewNames[2]=L"";previewNames[3]=L"";
    for(int i=0;i<FUNCTION_COUNT;i++){previewIcons[i]=NULL;previewOccupied[i]=i<2;}
    WCHAR system[512];GetSystemDirectoryW(system,512);
    for(int i=0;i<8;i++){
        int cell=i+FUNCTION_COUNT;previewNames[cell]=appNames[i];previewOccupied[cell]=TRUE;
        if(!appPaths[i][0]){previewIcons[cell]=NULL;continue;}
        WCHAR full[1024];swprintf(full,1024,L"%ls\\%ls",system,appPaths[i]);
        previewIcons[cell]=glassLoadIcon(full,appNames[i],full,96);
    }
    for(int i=12;i<COUNT;i++){int cell=i+FUNCTION_COUNT;previewNames[cell]=entries[i].name;previewIcons[cell]=NULL;previewOccupied[cell]=FALSE;}
}

static void render(const WCHAR *directory,const WCHAR *file,int theme,int renderDpi,int hoverCell,int withBackdrop){
    WCHAR path[2048];swprintf(path,2048,L"%ls\\%ls",directory,file);
    dpi=renderDpi;
    float scale=renderDpi/96.f;int width=(int)(PREVIEW_WIDTH*scale+.5f),height=(int)(PREVIEW_HEIGHT*scale+.5f);
    HDC screen=GetDC(NULL),mem=CreateCompatibleDC(screen);
    BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *pixels=NULL;HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&pixels,NULL,0);
    if(!bitmap){DeleteDC(mem);ReleaseDC(NULL,screen);return;}
    HGDIOBJ previous=SelectObject(mem,bitmap);
    if(hoverCell>=0){
        float pitch=(PREVIEW_WIDTH-24)/4.f;
        float x=12+(hoverCell%4)*pitch+pitch/2-4,y=(float)GLASS_GRID_TOP+(hoverCell/4)*GLASS_ROW_PITCH+46;
        glassInteraction(x,y,-1,-1);
    }else glassInteraction(-100,-100,-1,-1);
    /* Production blurs the live desktop behind the panel. Some renders capture
       the real screen; the rest keep the flat fallback so the panel can be
       judged at a known, reproducible backdrop luminance. */
    if(withBackdrop)glassCapture(0,0,width,height);
    glassTheme(theme);glassRows(PREVIEW_ROWS*4,0);
    glassPaint(mem,width,height,renderDpi,hoverCell,-1,FALSE,previewNames,previewIcons,previewOccupied);
    GdiFlush();glassMaskPixels(pixels,width,height,renderDpi);
    /* GDI+ flat API: this file is C, so no Gdiplus::Bitmap class here. */
    GpBitmap *image=NULL;BOOL saved=FALSE;
    if(GdipCreateBitmapFromScan0(width,height,width*4,PixelFormat32bppPARGB,(BYTE*)pixels,&image)==Ok){
        static const CLSID pngEncoder={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
        saved=GdipSaveImageToFile(image,path,&pngEncoder,NULL)==Ok;
        GdipDisposeImage((GpImage*)image);
    }
    if(!saved)wprintf(L"FAILED %ls\n",path);else wprintf(L"WROTE %ls (%dx%d)\n",path,width,height);
    SelectObject(mem,previous);DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(NULL,screen);
}

/* ------------------------------------------------------------ dialog shots */
/* The dialogs run their own modal loops, so a thread timer drives them: it
   captures the visible dialog and then closes it with IDCANCEL. No dialog is
   ever confirmed, so this never writes configuration or touches files. */
static const WCHAR *shotDirectory;
static int shotStage;

static void captureWindowPng(HWND w,const WCHAR *file){
    WCHAR path[2048];swprintf(path,2048,L"%ls\\%ls",shotDirectory,file);
    RECT r;GetWindowRect(w,&r);int width=r.right-r.left,height=r.bottom-r.top;
    if(width<=0||height<=0)return;
    HDC screen=GetDC(NULL),mem=CreateCompatibleDC(screen);
    BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *pixels=NULL;HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&pixels,NULL,0);
    if(!bitmap){DeleteDC(mem);ReleaseDC(NULL,screen);return;}
    HGDIOBJ previous=SelectObject(mem,bitmap);
    memset(pixels,0,(size_t)width*height*4);
    /* BitBlt from the screen DC, not PrintWindow: PrintWindow does not apply a
       child window's region, which is exactly what roundFieldControl() relies on. */
    SetStretchBltMode(mem,COLORONCOLOR);
    BitBlt(mem,0,0,width,height,screen,r.left,r.top,SRCCOPY);
    GdiFlush();
    GpBitmap *image=NULL;BOOL saved=FALSE;
    if(GdipCreateBitmapFromScan0(width,height,width*4,PixelFormat32bppPARGB,(BYTE*)pixels,&image)==Ok){
        static const CLSID pngEncoder={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
        saved=GdipSaveImageToFile(image,path,&pngEncoder,NULL)==Ok;
        GdipDisposeImage((GpImage*)image);
    }
    if(!saved)wprintf(L"FAILED %ls\n",path);else wprintf(L"WROTE %ls (%dx%d)\n",path,width,height);
    SelectObject(mem,previous);DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(NULL,screen);
}
static void CALLBACK shootAndClose(HWND w,UINT message,UINT_PTR timer,DWORD time){
    (void)w;(void)message;(void)time;
    KillTimer(NULL,timer);
    /* askScale() does not publish activeDialog, so fall back to a class lookup. */
    HWND dialog=activeDialog;
    if(!dialog||!IsWindowVisible(dialog)){
        const WCHAR *classes[]={L"LiteLaunch.Editor",L"LiteLaunch.Help",L"LiteLaunch.Scale",L"LiteLaunch.Blacklist"};
        for(int i=0;i<4&&!dialog;i++){HWND candidate=FindWindowW(classes[i],NULL);if(candidate&&IsWindowVisible(candidate))dialog=candidate;}
    }
    if(!dialog||!IsWindowVisible(dialog)){wprintf(L"MISSED stage %d\n",shotStage);return;}
    const WCHAR *names[]={L"dialog-editor.png",L"dialog-help.png",L"dialog-scale.png",L"dialog-blacklist.png"};
    captureWindowPng(dialog,shotStage<4?names[shotStage]:L"dialog-extra.png");
    shotStage++;
    SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);
}

int wmain(int argc,WCHAR **argv){
    if(argc<2){wprintf(L"usage: ui_preview.exe <output-directory>\n");return 2;}
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    glassInit();systemDpi=96;dpi=96;instance=GetModuleHandleW(NULL);
    dialogBackground=CreateSolidBrush(dialogColor);inputBackground=CreateSolidBrush(inputColor);
    updateFonts();
    fillFixture();
    /* Flat-backdrop renders first: once glassCapture runs, `backdrop` persists. */
    render(argv[1],L"panel-flat-theme0-96.png",0,96,-1,0);
    render(argv[1],L"panel-flat-theme0-hover-96.png",0,96,4,0);
    render(argv[1],L"panel-flat-theme0-function-hover-96.png",0,96,0,0);
    render(argv[1],L"panel-flat-theme9-96.png",9,96,-1,0);
    render(argv[1],L"panel-theme0-96.png",0,96,-1,1);
    render(argv[1],L"panel-theme0-hover-96.png",0,96,4,1);
    render(argv[1],L"panel-theme0-function-hover-96.png",0,96,0,1);
    render(argv[1],L"panel-theme1-96.png",1,96,-1,1);
    render(argv[1],L"panel-theme3-hover-96.png",3,96,5,1);
    render(argv[1],L"panel-theme9-96.png",9,96,-1,1);
    render(argv[1],L"panel-theme0-144.png",0,144,-1,1);
    render(argv[1],L"panel-theme3-192-hover.png",3,192,4,1);
    if(argc>=3&&!wcscmp(argv[2],L"--bench")){
        /* Frame cost of the production panel renderer, so a visual change can be
           checked against the previous version instead of guessed at. */
        dpi=96;glassTheme(0);glassRows(PREVIEW_ROWS*4,0);glassInteraction(-100,-100,-1,-1);
        int width=PREVIEW_WIDTH,height=PREVIEW_HEIGHT;
        HDC screen=GetDC(NULL),mem=CreateCompatibleDC(screen);
        BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;
        bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        void *pixels=NULL;HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&pixels,NULL,0);
        if(!bitmap){wprintf(L"BENCH failed\n");return 4;}
        HGDIOBJ previous=SelectObject(mem,bitmap);
        LARGE_INTEGER frequency,start,finish;QueryPerformanceFrequency(&frequency);
        const int iterations=300;
        QueryPerformanceCounter(&start);
        for(int i=0;i<iterations;i++){glassPaint(mem,width,height,96,-1,-1,FALSE,previewNames,previewIcons,previewOccupied);GdiFlush();glassMaskPixels(pixels,width,height,96);}
        QueryPerformanceCounter(&finish);
        double perFrame=1000.0*(finish.QuadPart-start.QuadPart)/frequency.QuadPart/iterations;
        wprintf(L"BENCH panel %dx%d paint+mask: %.2f ms/frame\n",width,height,perFrame);
        QueryPerformanceCounter(&start);
        for(int i=0;i<iterations;i++){glassPaint(mem,width,height,96,-1,-1,FALSE,previewNames,previewIcons,previewOccupied);GdiFlush();}
        QueryPerformanceCounter(&finish);
        wprintf(L"BENCH panel paint only: %.2f ms/frame\n",1000.0*(finish.QuadPart-start.QuadPart)/frequency.QuadPart/iterations);
        SelectObject(mem,previous);DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(NULL,screen);
    }
    if(argc>=3&&!wcscmp(argv[2],L"--menu")){
        /* A popup menu is owned by the thread that tracks it, so the production
           owner-draw handler is driven directly into a bitmap instead. */
        dpi=96;if(argc>=4)dpi=_wtoi(argv[3]);
        if(dpi<48||dpi>384)dpi=96;
        glassTheme(argc>=5?_wtoi(argv[4]):0);
        updateFonts();
        int rowHeight=S(34),separatorHeight=S(13),width=S(232);
        int themes=UI_THEME_COUNT,plain=6;
        int height=rowHeight*themes+separatorHeight+rowHeight*plain+S(8);
        HDC screen=GetDC(NULL),mem=CreateCompatibleDC(screen);
        BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;
        bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        void *pixels=NULL;HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&pixels,NULL,0);
        if(bitmap){
            HGDIOBJ previous=SelectObject(mem,bitmap);
            RECT all={0,0,width,height};HBRUSH background=CreateSolidBrush(inputColor);FillRect(mem,&all,background);DeleteObject(background);
            static MenuVisual visuals[UI_THEME_COUNT+8];
            const WCHAR *plainLabels[]={L"编辑应用…",L"固定面板",L"中键唤醒黑名单…",L"界面缩放",L"退出轻启",L""};
            int y=0,index=0;
            for(int i=0;i<themes;i++){
                visuals[index]=(MenuVisual){.label=uiThemeNames[i],.theme=i};
                DRAWITEMSTRUCT d={0};d.CtlType=ODT_MENU;d.hDC=mem;d.itemData=(ULONG_PTR)&visuals[index];
                d.itemState=(i==3)?ODS_CHECKED:(i==5?ODS_SELECTED:0);
                d.rcItem=(RECT){0,y,width,y+rowHeight};
                menuVisualDraw(&d);y+=rowHeight;index++;
            }
            for(int i=0;i<plain;i++){
                visuals[index]=(MenuVisual){.label=plainLabels[i],.theme=-1,.separator=(i==5)};
                DRAWITEMSTRUCT d={0};d.CtlType=ODT_MENU;d.hDC=mem;d.itemData=(ULONG_PTR)&visuals[index];
                d.itemState=(i==0)?ODS_SELECTED:0;
                int row=visuals[index].separator?separatorHeight:rowHeight;
                d.rcItem=(RECT){0,y,width,y+row};
                menuVisualDraw(&d);y+=row;index++;
            }
            GdiFlush();
            WCHAR path[2048];swprintf(path,2048,L"%ls\\menu.png",argv[1]);
            GpBitmap *image=NULL;BOOL saved=FALSE;
            if(GdipCreateBitmapFromScan0(width,height,width*4,PixelFormat32bppPARGB,(BYTE*)pixels,&image)==Ok){
                static const CLSID pngEncoder={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
                saved=GdipSaveImageToFile(image,path,&pngEncoder,NULL)==Ok;
                GdipDisposeImage((GpImage*)image);
            }
            wprintf(saved?L"WROTE %ls (%dx%d)\n":L"FAILED %ls\n",path,width,height);
            SelectObject(mem,previous);DeleteObject(bitmap);
        }
        DeleteDC(mem);ReleaseDC(NULL,screen);
    }
    if(argc>=3&&!wcscmp(argv[2],L"--dialogs")){
        shotDirectory=argv[1];shotStage=0;
        dpi=96;if(argc>=4)dpi=_wtoi(argv[3]);
        if(dpi<48||dpi>384)dpi=96;
        updateFonts();
        WNDCLASSW wc={0};wc.lpfnWndProc=PanelProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.style=0;wc.lpszClassName=CLASSNAME;
        RegisterClassW(&wc);
        wc.lpfnWndProc=EditProc;wc.lpszClassName=L"LiteLaunch.Editor";wc.hbrBackground=dialogBackground;RegisterClassW(&wc);
        panel=CreateWindowExW(WS_EX_TOPMOST,CLASSNAME,L"预览",WS_POPUP,0,0,S(panelWidth),S(PREVIEW_HEIGHT),NULL,NULL,instance,NULL);
        if(!panel){wprintf(L"FAILED panel window\n");return 3;}
        glassTheme(argc>=5?_wtoi(argv[4]):0);
        SetTimer(NULL,0,500,shootAndClose);editEntry(0);
        SetTimer(NULL,0,500,shootAndClose);showHelp();
        SetTimer(NULL,0,500,shootAndClose);askScale();
        SetTimer(NULL,0,500,shootAndClose);editBlacklist();
        DestroyWindow(panel);panel=NULL;
    }
    for(int i=0;i<CELL_COUNT;i++)if(previewIcons[i])glassFreeIcon(previewIcons[i]);
    if(dialogBackground)DeleteObject(dialogBackground);if(inputBackground)DeleteObject(inputBackground);
    glassShutdown();CoUninitialize();return 0;
}
