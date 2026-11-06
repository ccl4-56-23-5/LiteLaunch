/* Scale integration tests use production code with a disposable profile only. */
#define LITELAUNCH_CONFIG_TEST
#define wWinMain LiteLaunchUiEntry
#include "main.c"
#undef wWinMain
#define CHECK(x) do { if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} }while(0)
#define SCALE_NEAR(a,b) (fabs((double)(a)-(double)(b))<0.001)

typedef struct { DWORD size; ULONGLONG hash; FILETIME modified; } FileSnapshot;
static BOOL snapshotFile(LPCWSTR path,FileSnapshot *snapshot){
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return FALSE;
    BYTE buffer[4096];DWORD count=0;snapshot->size=0;snapshot->hash=14695981039346656037ULL;
    BOOL ok=GetFileTime(file,NULL,NULL,&snapshot->modified);
    while(ok){if(!ReadFile(file,buffer,sizeof(buffer),&count,NULL)){ok=FALSE;break;}if(!count)break;
        snapshot->size+=count;for(DWORD i=0;i<count;i++)snapshot->hash=(snapshot->hash^buffer[i])*1099511628211ULL;}
    CloseHandle(file);return ok;
}
static BOOL sameFile(const FileSnapshot *a,const FileSnapshot *b){
    return a->size==b->size&&a->hash==b->hash&&!CompareFileTime(&a->modified,&b->modified);
}
static BOOL writeStoredScale(LPCWSTR text){
    BOOL ok=WritePrivateProfileStringW(L"Panel",L"ScalePercent",text,config);
    WritePrivateProfileStringW(NULL,NULL,NULL,config);return ok;
}
static int dialogMode,dialogFailures;
static BOOL dialogSeen;
#define DIALOG_CHECK(x) do { if(!(x)){printf("FAIL dialog mode %d line %d: %s\n",dialogMode,__LINE__,#x);dialogFailures++;} }while(0)
static VOID CALLBACK verifyResetKey(HWND unused,UINT message,UINT_PTR timer,DWORD tick){
    (void)unused;(void)message;(void)tick;KillTimer(NULL,timer);
    DIALOG_CHECK(activeDialog&&IsWindow(activeDialog)&&!scaleAccepted&&SCALE_NEAR(scalePercent,100));
    if(activeDialog&&IsWindow(activeDialog))SendMessageW(activeDialog,WM_COMMAND,IDCANCEL,0);
}
static VOID CALLBACK driveScaleDialog(HWND unused,UINT message,UINT_PTR timer,DWORD tick){
    (void)unused;(void)message;(void)tick;
    HWND dialog=activeDialog;if(!dialog||!IsWindow(dialog))return;
    KillTimer(NULL,timer);dialogSeen=TRUE;HWND slider=GetDlgItem(dialog,202);
    RECT beforeRect,afterRect;GetWindowRect(dialog,&beforeRect);
    FileSnapshot before,after;DIALOG_CHECK(snapshotFile(config,&before));
    DIALOG_CHECK(slider&&SendMessageW(slider,TBM_GETRANGEMIN,0,0)==5000&&SendMessageW(slider,TBM_GETRANGEMAX,0,0)==15000);
    DIALOG_CHECK(SendMessageW(slider,TBM_GETLINESIZE,0,0)==100);
    const int values[]={5000,15000,7333,6250};
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){
        SendMessageW(slider,TBM_SETPOS,TRUE,values[i]);SendMessageW(dialog,WM_HSCROLL,TB_THUMBTRACK,(LPARAM)slider);
        DIALOG_CHECK(SCALE_NEAR(scalePercent,values[i]/100.0));GetWindowRect(dialog,&afterRect);
        DIALOG_CHECK(EqualRect(&beforeRect,&afterRect));
        WCHAR text[32];GetDlgItemTextW(dialog,201,text,32);DIALOG_CHECK(SCALE_NEAR(wcstod(text,NULL),values[i]/100.0));
    }
    DIALOG_CHECK(snapshotFile(config,&after)&&sameFile(&before,&after));
    SendMessageW(dialog,WM_COMMAND,203,0);DIALOG_CHECK(SCALE_NEAR(scalePercent,100));
    DIALOG_CHECK(SendMessageW(slider,TBM_GETPOS,0,0)==10000);
    SetDlgItemTextW(dialog,201,L"bad");DIALOG_CHECK(SCALE_NEAR(scalePercent,100));
    SetDlgItemTextW(dialog,201,L"49.99");DIALOG_CHECK(SCALE_NEAR(scalePercent,100));
    SetDlgItemTextW(dialog,201,L"73.33");DIALOG_CHECK(SCALE_NEAR(scalePercent,73.33));
    DIALOG_CHECK(SendMessageW(slider,TBM_GETPOS,0,0)==7333);
    if(dialogMode==1)SendMessageW(dialog,WM_COMMAND,IDOK,0);
    else if(dialogMode==2)PostMessageW(dialog,WM_KEYDOWN,VK_ESCAPE,0);
    else if(dialogMode==3)SendMessageW(dialog,WM_CLOSE,0,0);
    else if(dialogMode==4){
        WCHAR originalTemp[MAX_PATH];lstrcpyW(originalTemp,configTemp);lstrcpyW(configTemp,L"?:\\LiteLaunch-test-invalid\\save.tmp");
        SendMessageW(dialog,WM_COMMAND,IDOK,0);DIALOG_CHECK(IsWindow(dialog)&&!scaleAccepted);
        lstrcpyW(configTemp,originalTemp);SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);
    }else if(dialogMode==5){
        SetFocus(GetDlgItem(dialog,IDCANCEL));DIALOG_CHECK(GetFocus()==GetDlgItem(dialog,IDCANCEL));
        PostMessageW(dialog,WM_KEYDOWN,VK_RETURN,0);
    }else if(dialogMode==6){
        SetFocus(GetDlgItem(dialog,203));DIALOG_CHECK(GetFocus()==GetDlgItem(dialog,203));
        DIALOG_CHECK(SetTimer(NULL,0,50,verifyResetKey));PostMessageW(dialog,WM_KEYDOWN,VK_RETURN,0);
    }else SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);
}

int wmain(int argc,WCHAR **argv){
    CHECK(argc==2);SetProcessDPIAware();instance=GetModuleHandleW(NULL);
    CHECK(SUCCEEDED(CoInitializeEx(NULL,COINIT_APARTMENTTHREADED)));glassInit();
    CHECK(setConfigPaths(argv[1],argv[1]));
    /* The fixture intentionally has no real executable targets or plugins. */
    ZeroMemory(entries,sizeof(entries));lstrcpyW(entries[0].name,L"缩放测试 中文");blacklistCount=0;
    scalePercent=100;systemDpi=96;dpi=96;CHECK(saveConfig());CHECK(validConfig(config));

    const struct { LPCWSTR text; double expected; } stored[]={
        {L"50",50},{L"62.5",62.5},{L"73.33",73.33},{L"100",100},{L"150",150},
        {L"60",60},{L"200",150},{L"25",50},{L"999",150},{L"-5",50},
        {L"73.336",73.34},{L"bad",100},{L"",100},{L"NaN",100},{L"inf",100},
        {L"1e999",100},{L"80xyz",100},{L"75%",100},{L" 85.25 ",85.25}
    };
    for(unsigned i=0;i<sizeof(stored)/sizeof(stored[0]);i++){
        CHECK(writeStoredScale(stored[i].text));CHECK(loadConfig());CHECK(SCALE_NEAR(scalePercent,stored[i].expected));
        CHECK(SCALE_NEAR(dpi,systemDpi*stored[i].expected/100.0));
    }
    puts("PASS 19 stored-scale cases: decimals, legacy integers, bounds, malformed and nonfinite values");
    const double percentages[]={50,62.5,73.33,100,150};
    for(unsigned i=0;i<sizeof(percentages)/sizeof(percentages[0]);i++){
        scalePercent=percentages[i];CHECK(saveConfig());scalePercent=1;CHECK(loadConfig());CHECK(SCALE_NEAR(scalePercent,percentages[i]));
        CHECK(!wcscmp(entries[0].name,L"缩放测试 中文"));
    }
    puts("PASS 5 fractional-scale persistence round trips retain unrelated Unicode entry data");

    WNDCLASSW cls={0};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=instance;cls.lpszClassName=L"LiteLaunch.ScaleRegression";
    CHECK(RegisterClassW(&cls));
    panel=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW,cls.lpszClassName,L"Scale regression fixture",WS_POPUP,30,40,448,524,NULL,NULL,instance,NULL);
    CHECK(panel);dialogBackground=CreateSolidBrush(dialogColor);inputBackground=CreateSolidBrush(inputColor);
    const int dpis[]={96,120,144,192};
    FileSnapshot before,after,backupBefore,backupAfter;CHECK(snapshotFile(config,&before));CHECK(snapshotFile(configBackup,&backupBefore));
    for(unsigned d=0;d<sizeof(dpis)/sizeof(dpis[0]);d++)for(unsigned s=0;s<sizeof(percentages)/sizeof(percentages[0]);s++){
        systemDpi=dpis[d];previewScale(percentages[s]);
        RECT r;CHECK(GetWindowRect(panel,&r));CHECK(r.right-r.left==(int)lround(448*systemDpi*percentages[s]/9600.0));
        CHECK(r.bottom-r.top==S(GLASS_GRID_TOP+visibleRows*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT));
        CHECK(SCALE_NEAR(dpi,systemDpi*percentages[s]/100.0));
        scrollRow=0;
        for(int row=0;row<visibleRows;row++)for(int col=0;col<4;col++){
            int x=S(12+col*106+53),y=S(GLASS_GRID_TOP+row*GLASS_ROW_PITCH+46);
            CHECK(cellAt(x,y)==row*4+col);
        }
        CHECK(cellAt(S(4),S(GLASS_GRID_TOP+10))==-1);
        CHECK(cellAt(S(panelWidth-4),S(GLASS_GRID_TOP+10))==-1);
        CHECK(cellAt(S(30),S(GLASS_GRID_TOP-8))==-1);
        CHECK(cellAt(S(30),S(GLASS_GRID_TOP+visibleRows*GLASS_ROW_PITCH+8))==-1);
        /* Paint at the same fractional DPI that drives hit testing. */
        paint(panel);CHECK(lastLayeredPaintOk);
    }
    CHECK(snapshotFile(config,&after));CHECK(sameFile(&before,&after));
    CHECK(snapshotFile(configBackup,&backupAfter));CHECK(sameFile(&backupBefore,&backupAfter));
    puts("PASS 20 DPI/scale combinations: exact physical dimensions, cell hit mapping, layered paint, preview performs no profile writes");

    systemDpi=96;previewScale(100);int widths=0,last=-1;
    for(int step=5000;step<=15000;step++){
        double value=step/100.0;dpi=(float)(systemDpi*value/100.0);int current=S(panelWidth);
        CHECK(current>=last);if(last>=0)CHECK(current-last<=1);if(current!=last)widths++;last=current;
    }
    CHECK(widths==449);previewScale(100);
    puts("PASS 10001 slider values cover every physical panel width from 224 through 672 without multi-pixel jumps");

    /* Exercise actual native controls and the production modal message loop. */
    for(dialogMode=0;dialogMode<=6;dialogMode++){
        lstrcpyW(entries[59].target,L"fixture.exe");previewScale(100);scrollRow=2;
        glassRows(displayRows*4,scrollRow);RECT originalRect,restoredRect;CHECK(GetWindowRect(panel,&originalRect));
        CHECK(snapshotFile(config,&before));CHECK(snapshotFile(configBackup,&backupBefore));
        dialogSeen=FALSE;CHECK(SetTimer(NULL,0,50,driveScaleDialog));askScale();CHECK(dialogSeen&&!dialogFailures);
        CHECK(!activeDialog&&!editing&&IsWindowEnabled(panel));
        if(dialogMode==1){CHECK(scaleAccepted&&SCALE_NEAR(scalePercent,73.33));WCHAR storedText[32];GetPrivateProfileStringW(L"Panel",L"ScalePercent",L"",storedText,32,config);CHECK(SCALE_NEAR(wcstod(storedText,NULL),73.33));}
        else{
            CHECK(!scaleAccepted&&SCALE_NEAR(scalePercent,100)&&scrollRow==2);CHECK(GetWindowRect(panel,&restoredRect));CHECK(EqualRect(&originalRect,&restoredRect));
            CHECK(snapshotFile(config,&after)&&sameFile(&before,&after));CHECK(snapshotFile(configBackup,&backupAfter)&&sameFile(&backupBefore,&backupAfter));
        }
        entries[59].target[0]=0;
    }
    puts("PASS native slider range, live preview, fixed-size dialog, edit synchronization, reset and successful apply");
    puts("PASS Cancel, Escape, Close and failed-save cancellation restore scale, scroll and window rectangle without profile writes");
    puts("PASS Enter invokes the focused Cancel or Reset button rather than implicitly applying the preview");

    applyScale(73.33);CHECK(SCALE_NEAR(scalePercent,73.33));CHECK(loadConfig());CHECK(SCALE_NEAR(scalePercent,73.33));
    CHECK(snapshotFile(config,&before));applyScale(49.99);applyScale(150.01);applyScale(NAN);applyScale(INFINITY);
    CHECK(SCALE_NEAR(scalePercent,73.33));CHECK(snapshotFile(config,&after));CHECK(sameFile(&before,&after));
    puts("PASS apply persists fractional percentage and rejects out-of-range/nonfinite requests without touching profile");

    DestroyWindow(panel);panel=NULL;DeleteObject(dialogBackground);DeleteObject(inputBackground);
    DeleteObject(font);DeleteObject(titleFont);DeleteObject(symbolFont);DeleteObject(metaFont);DeleteObject(menuFont);
    for(int i=0;i<COUNT;i++){if(entries[i].visual)glassFreeIcon(entries[i].visual);if(entries[i].image)DestroyIcon(entries[i].image);}
    glassShutdown();CoUninitialize();puts("ALL SCALE REGRESSIONS PASSED");return 0;
}
