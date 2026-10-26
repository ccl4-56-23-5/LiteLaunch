/* Integration checks use the actual production save/load and launch functions. */
#define wWinMain LiteLaunchUiEntry
#include "main.c"
#undef wWinMain
#define CHECK(x) do { if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} }while(0)
static BOOL editorSeen,edgeFocusSeen,blacklistSeen,automaticAddSaved,duplicateIgnored,cancelPreserved;
static VOID CALLBACK closeFixture(HWND w,UINT message,UINT_PTR timer,DWORD time){
    (void)w;(void)message;(void)time;
    if(activeDialog&&IsWindowVisible(activeDialog)){
        WCHAR title[100];GetWindowTextW(activeDialog,title,100);
        if(wcsstr(title,L"添加")){
            editorSeen=IsWindow(fields[0])&&IsWindow(fields[1]);edgeFocusSeen=editorSeen;
            const int fieldTop[4]={92,166,236,306};
            for(int i=0;i<4;i++){
                SendMessageW(activeDialog,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(S(45),S(fieldTop[i]+1)));
                if(GetFocus()!=fields[i])edgeFocusSeen=FALSE;
            }
        }
        if(wcsstr(title,L"黑名单")){
            blacklistSeen=IsWindow(blacklistList);
            blacklistRecognized(TRUE,L"C:\\Fixture\\test.exe",L"C:\\Fixture\\test.exe",L"");
            WCHAR stored[2048];GetPrivateProfileStringW(L"Blacklist",L"App0",L"",stored,2048,config);
            automaticAddSaved=blacklistCount==1&&SendMessageW(blacklistList,LB_GETCOUNT,0,0)==1&&GetPrivateProfileIntW(L"Blacklist",L"Count",0,config)==1&&!wcscmp(stored,L"C:\\Fixture\\test.exe");
            blacklistRecognized(TRUE,L"C:\\Fixture\\test.exe",L"C:\\Fixture\\test.exe",L"");duplicateIgnored=blacklistCount==1;
            blacklistRecognized(FALSE,L"",L"",L"已取消");cancelPreserved=blacklistCount==1;
            SendMessageW(blacklistList,LB_SETCURSEL,blacklistCount-1,0);SendMessageW(activeDialog,WM_COMMAND,105,0);
        }
        SendMessageW(activeDialog,WM_COMMAND,IDCANCEL,0);KillTimer(NULL,timer);
    }
}
int wmain(int argc,WCHAR **argv) {
    if(argc==3&&!wcscmp(argv[1],L"--probe")) {FILE *f=_wfopen(argv[2],L"w");if(!f)return 2;fputs("launched",f);fclose(f);return 0;}
    CHECK(argc>=2);
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    glassInit();
    CHECK(setConfigPaths(argv[1],argv[1]));
    lstrcpyW(entries[0].name,L"测试应用 中文");lstrcpyW(entries[0].target,L"shell:AppsFolder\\OpenAI.Codex\\_2p2nqsd0c76g0!App");
    normalize(entries[0].target);CHECK(!wcscmp(entries[0].target,L"shell:AppsFolder\\OpenAI.Codex_2p2nqsd0c76g0!App"));
    themeIndex=9;scalePercent=135;lstrcpyW(entries[0].args,L"--title \"中文 空格\"");CHECK(saveConfig());themeIndex=1;scalePercent=100;
    CHECK(validConfig(config));CHECK(validConfig(configBackup));
    ZeroMemory(entries,sizeof(entries));CHECK(loadConfig());
    CHECK(themeIndex==9&&scalePercent==135&&dpi==MulDiv(systemDpi,135,100));themeIndex=1;glassTheme(1);
    CHECK(!wcscmp(entries[0].name,L"测试应用 中文"));CHECK(!wcscmp(entries[0].args,L"--title \"中文 空格\""));
    printf("PASS Unicode configuration and escaped package identifier round-trip\n");
    WCHAR shortcut[2048],self[2048];GetModuleFileNameW(NULL,self,2048);
    WNDCLASSW existingClass={0};existingClass.lpfnWndProc=DefWindowProcW;existingClass.hInstance=GetModuleHandleW(NULL);
    existingClass.lpszClassName=L"LiteLaunch.HiddenTargetFixture";CHECK(RegisterClassW(&existingClass));
    HWND hidden=CreateWindowW(existingClass.lpszClassName,L"Existing application window",WS_OVERLAPPEDWINDOW,0,0,240,160,NULL,NULL,existingClass.hInstance,NULL);
    CHECK(hidden&&!IsWindowVisible(hidden));CHECK(hiddenTargetWindow(self)==hidden);
    CHECK(revealTargetWindow(hidden,2)&&IsWindowVisible(hidden));DestroyWindow(hidden);
    puts("PASS hidden existing application window is located and made visible");
    swprintf(shortcut,2048,L"%ls\\快捷方式验证.lnk",argv[1]);
    IShellLinkW *link=NULL;IPersistFile *persist=NULL;
    CHECK(SUCCEEDED(CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link)));
    CHECK(SUCCEEDED(IShellLinkW_SetPath(link,self)));
    CHECK(SUCCEEDED(IShellLinkW_SetArguments(link,L"--probe \"参数 空格\"")));
    CHECK(SUCCEEDED(IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&persist)));
    CHECK(SUCCEEDED(persist->lpVtbl->Save(persist,shortcut,TRUE)));
    persist->lpVtbl->Release(persist);IShellLinkW_Release(link);
    Entry resolved={0};CHECK(profileFromPath(shortcut,&resolved));
    CHECK(!_wcsicmp(resolved.target,self));CHECK(!wcscmp(resolved.args,L"--probe \"参数 空格\""));
    CHECK(!wcscmp(resolved.name,L"快捷方式验证"));CHECK(_wcsicmp(resolved.target,shortcut));
    printf("PASS shortcut resolves application target, arguments and display name\n");
    Entry probe={0};GetModuleFileNameW(NULL,probe.target,2048);WCHAR marker[2048];swprintf(marker,2048,L"%ls\\启动验证-%lu.txt",argv[1],GetCurrentProcessId());
    swprintf(probe.args,2048,L"--probe \"%ls\"",marker);DWORD failure;
    CHECK(startTarget(&probe,&failure));
    for(int i=0;i<200&&GetFileAttributesW(marker)==INVALID_FILE_ATTRIBUTES;i++)Sleep(10);
    CHECK(GetFileAttributesW(marker)!=INVALID_FILE_ATTRIBUTES);printf("PASS executable launch with Unicode path and separate arguments\n");
    if(argc>=3&&!wcscmp(argv[2],L"--activate-codex")) {
        Entry codex={0};lstrcpyW(codex.target,L"shell:AppsFolder\\OpenAI.Codex_2p2nqsd0c76g0!App");
        LARGE_INTEGER a,b,f;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
        BOOL launched=startTarget(&codex,&failure);QueryPerformanceCounter(&b);
        printf("Codex activation: success=%d result=0x%08lX call_ms=%.2f\n",launched,failure,1000.0*(b.QuadPart-a.QuadPart)/f.QuadPart);
        CHECK(launched);
    }
    WNDCLASSW renderClass={0};renderClass.lpfnWndProc=DefWindowProcW;renderClass.hInstance=GetModuleHandleW(NULL);renderClass.lpszClassName=L"LiteLaunch.RenderCheck";CHECK(RegisterClassW(&renderClass));
    int scales[]={96,120,144,192};
    for(int k=0;k<4;k++){
        dpi=scales[k];HWND render=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW,renderClass.lpszClassName,L"Render check",WS_POPUP,0,0,S(panelWidth),S(GLASS_GRID_TOP+4*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT),NULL,NULL,renderClass.hInstance,NULL);CHECK(render);
        glassCapture(0,0,S(panelWidth),S(GLASS_GRID_TOP+4*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT));paint(render);CHECK(lastLayeredPaintOk);
        CHECK(cellAt(S(20),S(GLASS_GRID_TOP+12))==0);CHECK(cellAt(S(20),S(GLASS_GRID_TOP+GLASS_ROW_PITCH+12))==4);CHECK(cellAt(S(panelWidth-30),S(292+GLASS_ROW_PITCH))==15);CHECK(cellAt(S(panelWidth-2),S(GLASS_GRID_TOP+12))==-1);
        DestroyWindow(render);
    }
    printf("PASS per-pixel layered rendering and grid hit targets at 100/125/150/200 percent DPI\n");
    dpi=96;panel=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW,renderClass.lpszClassName,L"Drag test",WS_POPUP,0,0,S(panelWidth),S(GLASS_GRID_TOP+4*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT),NULL,NULL,renderClass.hInstance,NULL);CHECK(panel);
    CHECK(configurePanelCapture());DWORD affinity=0xFFFFFFFF;CHECK(GetWindowDisplayAffinity(panel,&affinity)&&affinity==WDA_NONE&&!liveCapture);
    puts("PASS production panel policy permits screen capture and disables visible-background feedback");
    WCHAR original[128];lstrcpyW(original,entries[0].name);
    const int column=S((panelWidth-24)/4),thirdX=S(12+((panelWidth-24)/4)*2+20),topY=S(GLASS_GRID_TOP+GLASS_ROW_PITCH+12),functionY=S(GLASS_GRID_TOP+12),appX=S(30)+column,fourthX=S(30)+column*3;
    PanelProc(panel,WM_LBUTTONDOWN,0,MAKELPARAM(appX,topY));CHECK(pressedIndex==5);
    PanelProc(panel,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(fourthX,topY));CHECK(iconDragging);
    PanelProc(panel,WM_LBUTTONUP,0,MAKELPARAM(fourthX,topY));CHECK(!wcscmp(entries[2].name,original));CHECK(pressedIndex==-1&&!iconDragging);
    WCHAR diskName[128];GetPrivateProfileStringW(L"Item2",L"Name",L"",diskName,128,config);CHECK(!wcscmp(diskName,original));
    CHECK(reorderEntry(2,0));CHECK(!wcscmp(entries[0].name,original));
    static Entry beforeBlockedDrag[COUNT];memcpy(beforeBlockedDrag,entries,sizeof(entries));WIN32_FILE_ATTRIBUTE_DATA beforeConfig,afterConfig;CHECK(GetFileAttributesExW(config,GetFileExInfoStandard,&beforeConfig));
    PanelProc(panel,WM_LBUTTONDOWN,0,MAKELPARAM(appX,topY));PanelProc(panel,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(S(30),functionY));CHECK(iconDragging);
    PanelProc(panel,WM_LBUTTONUP,0,MAKELPARAM(S(30),functionY));CHECK(!memcmp(beforeBlockedDrag,entries,sizeof(entries)));CHECK(GetFileAttributesExW(config,GetFileExInfoStandard,&afterConfig));CHECK(!CompareFileTime(&beforeConfig.ftLastWriteTime,&afterConfig.ftLastWriteTime));
    CHECK(appIndexForCell(4)==-1&&appIndexForCell(5)==0);CHECK(selectableCell(0)&&selectableCell(1)&&selectableCell(2)&&selectableCell(3)&&selectableCell(4));
    PanelProc(panel,WM_LBUTTONDOWN,0,MAKELPARAM(thirdX,functionY));CHECK(pressedIndex==2);PanelProc(panel,WM_CANCELMODE,0,0);
    selection=1;PanelProc(panel,WM_KEYDOWN,VK_RIGHT,0);CHECK(selection==2);PanelProc(panel,WM_KEYDOWN,VK_LEFT,0);CHECK(selection==1);
    selection=7;PanelProc(panel,WM_KEYDOWN,VK_UP,0);CHECK(selection==3);
    selection=6;hover=-1;PanelProc(panel,WM_MOUSEMOVE,0,MAKELPARAM(1,35));CHECK(selection==-1&&hover==-1);
    DestroyWindow(panel);panel=NULL;printf("PASS drag reorder and persistence preserve fixed Control Panel and plugin row\n");
    puts("PASS four plugin slots are selectable; fixed Control Panel is not a mutable app entry");
    blacklistCount=3;lstrcpyW(blacklist[0],L"BLENDER.exe");lstrcpyW(blacklist[1],L"D:\\Apps\\specific.exe");lstrcpyW(blacklist[2],L"Vendor.App_xyz!App");
    CHECK(blacklistMatches(L"C:\\tools\\blender.exe",L""));CHECK(blacklistMatches(L"d:\\apps\\specific.exe",L""));CHECK(blacklistMatches(L"",L"Vendor.App_xyz!App"));
    CHECK(!blacklistMatches(L"C:\\elsewhere\\specific.exe",L""));CHECK(!blacklistMatches(L"C:\\tools\\notblender.exe",L""));
    CHECK(saveConfig());blacklistCount=0;CHECK(loadConfig());dpi=96;CHECK(blacklistCount==3);CHECK(blacklistMatches(L"C:\\tools\\blender.exe",L""));
    printf("PASS blacklist exact path, process name, app ID, case folding and persistence\n");
    blacklistCount=1;blacklist[0][0]=0;CHECK(!blacklistMatches(L"",L""));blacklistCount=0;
    instance=GetModuleHandleW(NULL);WNDCLASSW ec={0};ec.hInstance=instance;ec.lpfnWndProc=EditProc;ec.lpszClassName=L"LiteLaunch.Editor";RegisterClassW(&ec);
    panel=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW,renderClass.lpszClassName,L"Input fixture",WS_POPUP,0,0,S(panelWidth),S(GLASS_GRID_TOP+4*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT),NULL,NULL,instance,NULL);
    const int lastX=S(panelWidth-50),thirdY=S(GLASS_GRID_TOP+3*GLASS_ROW_PITCH+14);
    PanelProc(panel,WM_LBUTTONDOWN,0,MAKELPARAM(lastX,thirdY));PanelProc(panel,WM_CAPTURECHANGED,0,0);CHECK(pressedIndex==15);
    PanelProc(panel,WM_LBUTTONUP,0,MAKELPARAM(lastX,thirdY));MSG queued;CHECK(PeekMessageW(&queued,panel,WM_OPENENTRY,WM_OPENENTRY,PM_REMOVE));
    SetTimer(NULL,0,100,closeFixture);PanelProc(panel,queued.message,queued.wParam,queued.lParam);CHECK(editorSeen&&edgeFocusSeen&&!editing&&!activeDialog);
    SetTimer(NULL,0,100,closeFixture);editBlacklist();CHECK(blacklistSeen&&!editing&&!activeDialog&&!picking&&blacklistCount==0);DestroyWindow(panel);panel=NULL;
    CHECK(automaticAddSaved&&duplicateIgnored&&cancelPreserved);CHECK(loadConfig());dpi=96;CHECK(blacklistCount==0);
    puts("PASS blacklist selection immediately saves and populates list; duplicate ignored; cancel preserves entries; removal persists");
    puts("PASS empty-slot click opens editor; blacklist list add/remove and modal cleanup; blank rule never blocks all apps");
    for(int i=0;i<COUNT;i++)entries[i].target[0]=0;
    CHECK(neededRows()==3);for(int i=0;i<12;i++)lstrcpyW(entries[i].target,L"fixture.exe");CHECK(neededRows()==4);
    entries[1].target[0]=0;CHECK(neededRows()==4);lstrcpyW(entries[1].target,L"fixture.exe");CHECK(neededRows()==4);
    for(int i=12;i<16;i++)lstrcpyW(entries[i].target,L"fixture.exe");CHECK(neededRows()==5);
    entries[4].target[0]=0;CHECK(neededRows()==5);lstrcpyW(entries[4].target,L"fixture.exe");CHECK(neededRows()==5);
    displayRows=6;visibleRows=4;scrollRow=1;CHECK(cellAt(lastX,thirdY)==19);CHECK(cellAt(S(30),functionY)==0);CHECK(cellAt(thirdX,functionY)==2);CHECK(cellAt(S(30),topY)==8);
    displayRows=34;scrollRow=30;CHECK(cellAt(lastX,thirdY)==-1);CHECK(cellAt(S(30),thirdY)==132);CHECK(appIndexForCell(132)==127);CHECK(cellAt(S(30),functionY)==0);CHECK(appIndexForCell(0)==-1&&appIndexForCell(3)==-1&&appIndexForCell(4)==-1&&appIndexForCell(5)==0);
    scrollRow=0;displayRows=4;CHECK(cellAt(S(30),topY)==4);
    CHECK(saveConfig());entries[15].target[0]=0;CHECK(loadConfig());CHECK(!wcscmp(entries[15].target,L"fixture.exe"));
    puts("PASS full third/fourth rows expand, scroll hit mapping and additional entries persist");
    puts("PASS fixed function hit row at scroll offsets; first and 128th apps map to cells 5 and 132");
    panel=CreateWindowW(renderClass.lpszClassName,L"Shortcut drop fixture",WS_POPUP,0,0,1,1,NULL,NULL,instance,NULL);CHECK(panel);
    SIZE_T pathBytes=(wcslen(shortcut)+2)*sizeof(WCHAR),dropBytes=sizeof(DROPFILES)+pathBytes;
    HDROP drop=(HDROP)GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,dropBytes);CHECK(drop);
    DROPFILES *dropData=(DROPFILES*)GlobalLock(drop);CHECK(dropData);dropData->pFiles=sizeof(DROPFILES);dropData->fWide=TRUE;
    memcpy((BYTE*)dropData+sizeof(DROPFILES),shortcut,(wcslen(shortcut)+1)*sizeof(WCHAR));GlobalUnlock(drop);
    PanelProc(panel,WM_DROPFILES,(WPARAM)drop,0);
    CHECK(!_wcsicmp(entries[16].target,self));CHECK(!wcscmp(entries[16].args,L"--probe \"参数 空格\""));
    CHECK(!wcscmp(entries[16].name,L"快捷方式验证"));
    applyScale(150);RECT scaled;GetWindowRect(panel,&scaled);
    CHECK(scalePercent==150&&dpi==MulDiv(systemDpi,150,100));
    CHECK(scaled.right-scaled.left==S(panelWidth));
    CHECK(GetPrivateProfileIntW(L"Panel",L"ScalePercent",0,config)==150);
    DestroyWindow(panel);panel=NULL;
    puts("PASS dropped shortcut stores resolved application instead of shortcut path");
    puts("PASS custom scale updates panel width and persists");
    for(int i=0;i<COUNT;i++)if(entries[i].visual)glassFreeIcon(entries[i].visual);
    glassShutdown();CoUninitialize();return 0;
}
