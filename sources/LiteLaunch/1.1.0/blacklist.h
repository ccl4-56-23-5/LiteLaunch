/* UI edits are published under a short lock; the mouse hook never waits on disk I/O. */
static BOOL blacklistMatches(LPCWSTR path,LPCWSTR appId){
    const WCHAR *base=wcsrchr(path,L'\\');base=base?base+1:path;BOOL blocked=FALSE;
    AcquireSRWLockShared(&blacklistLock);
    for(int i=0;i<blacklistCount;i++)if(blacklist[i][0]&&((path[0]&&(!_wcsicmp(blacklist[i],path)||!_wcsicmp(blacklist[i],base)))||(appId[0]&&!_wcsicmp(blacklist[i],appId)))){blocked=TRUE;break;}
    ReleaseSRWLockShared(&blacklistLock);return blocked;
}
static LPCWSTR processBaseName(LPCWSTR path){const WCHAR *base=wcsrchr(path,92);return base?base+1:path;}
static BOOL uuProcess(LPCWSTR path){
    LPCWSTR base=processBaseName(path);
    return !_wcsicmp(base,L"GameViewer.exe")||!_wcsicmp(base,L"GameViewerServer.exe");
}
/* A full-path UU rule expands only within the same installation directory. */
static BOOL sameUuInstall(LPCWSTR rule,LPCWSTR path){
    if(!uuProcess(rule)||!uuProcess(path))return FALSE;
    LPCWSTR ruleBase=processBaseName(rule),pathBase=processBaseName(path);
    if(ruleBase==rule)return TRUE;
    size_t n=(size_t)(ruleBase-rule);
    return (size_t)(pathBase-path)==n&&!_wcsnicmp(rule,path,n);
}
static int mouseBlacklistKind(LPCWSTR path,LPCWSTR appId,DWORD flags){
    int result=0;(void)flags;
    AcquireSRWLockShared(&blacklistLock);
    for(int i=0;i<blacklistCount;i++){
        LPCWSTR rule=blacklist[i];if(!rule[0])continue;
        BOOL match=(path[0]&&(!_wcsicmp(rule,path)||!_wcsicmp(rule,processBaseName(path))))||(appId[0]&&!_wcsicmp(rule,appId));
        BOOL uu=sameUuInstall(rule,path);
        if(!match&&!uu)continue;
        if(uu){if(!_wcsicmp(processBaseName(path),L"GameViewer.exe"))result=2;}
        else {result=1;break;}
    }
    ReleaseSRWLockShared(&blacklistLock);return result;
}
/* UU's SYSTEM window server can reject PROCESS_QUERY_LIMITED_INFORMATION.
   Toolhelp exposes its exact image name without opening the protected process.
   This fallback intentionally treats inaccessible UU installations as one app. */
static int inaccessibleUuWindow(DWORD pid,DWORD flags){
    (void)flags;
    static DWORD cachedPid,cachedAt;static WCHAR cachedName[MAX_PATH];
    DWORD now=GetTickCount();
    if(cachedPid!=pid||now-cachedAt>=2000){
        cachedPid=pid;cachedAt=now;cachedName[0]=0;
        HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(snapshot!=INVALID_HANDLE_VALUE){
            PROCESSENTRY32W entry={0};entry.dwSize=sizeof(entry);
            if(Process32FirstW(snapshot,&entry))do{if(entry.th32ProcessID==pid){lstrcpynW(cachedName,entry.szExeFile,MAX_PATH);break;}}while(Process32NextW(snapshot,&entry));
            CloseHandle(snapshot);
        }
    }
    if(_wcsicmp(cachedName,L"GameViewer.exe"))return 0;
    int result=0;
    AcquireSRWLockShared(&blacklistLock);
    for(int i=0;i<blacklistCount;i++)if(uuProcess(blacklist[i])){result=2;break;}
    ReleaseSRWLockShared(&blacklistLock);return result;
}
static int blockedWindow(HWND window,DWORD flags){
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(!pid||pid==GetCurrentProcessId())return 0;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return inaccessibleUuWindow(pid,flags);
    WCHAR path[2048]=L"",appId[2048]=L"";DWORD count=2048;BOOL pathRead=QueryFullProcessImageNameW(process,0,path,&count);
    UINT32 len=2048;GetApplicationUserModelId(process,&len,appId);CloseHandle(process);
    int kind=mouseBlacklistKind(path,appId,flags);return kind?kind:(!pathRead?inaccessibleUuWindow(pid,flags):0);
}
typedef struct {DWORD flags;int result;} MouseBlacklistScan;
static BOOL CALLBACK blockedChild(HWND child,LPARAM data){MouseBlacklistScan *scan=(MouseBlacklistScan*)data;int kind=blockedWindow(child,scan->flags);if(kind==1||!scan->result)scan->result=kind;return scan->result!=1;}
static int blockedAtPoint(POINT point,DWORD flags){
    AcquireSRWLockShared(&blacklistLock);BOOL any=blacklistCount>0;ReleaseSRWLockShared(&blacklistLock);if(!any)return 0;
    HWND hit=WindowFromPoint(point),root=GetAncestor(hit,GA_ROOT);DWORD pid=0;GetWindowThreadProcessId(root,&pid);
    if(pid==GetCurrentProcessId())return 0;
    if(!hit)return blockedWindow(GetForegroundWindow(),flags);
    MouseBlacklistScan scan={flags,0};blockedChild(hit,(LPARAM)&scan);if(scan.result==1)return 1;
    if(root&&root!=hit)blockedChild(root,(LPARAM)&scan);if(scan.result==1)return 1;
    /* Owned viewer popups may live in another process. */
    HWND owner=root?GetWindow(root,GW_OWNER):NULL;for(int i=0;owner&&i<8;i++){blockedChild(owner,(LPARAM)&scan);if(scan.result==1)return 1;owner=GetWindow(owner,GW_OWNER);}
    // Hosted Store apps can run in a child process of ApplicationFrameHost.
    if(root)EnumChildWindows(root,blockedChild,(LPARAM)&scan);return scan.result;
}

static HWND blacklistList,blacklistStatus,blacklistAddButton;

static WCHAR blacklistCandidate[2048];
static void setBlacklistStatus(LPCWSTR value){SetWindowTextW(blacklistStatus,value);InvalidateRect(blacklistStatus,NULL,TRUE);}
static void refreshBlacklist(void){
    SendMessageW(blacklistList,LB_RESETCONTENT,0,0);
    HDC dc=GetDC(blacklistList);HGDIOBJ old=dc&&font?SelectObject(dc,font):NULL;int extent=0;
    for(int i=0;i<blacklistCount;i++){
        WCHAR label[2300];const WCHAR *base=wcsrchr(blacklist[i],92);base=base?base+1:blacklist[i];
        swprintf(label,2300,L"%ls  —  %ls",base,blacklist[i]);SendMessageW(blacklistList,LB_ADDSTRING,0,(LPARAM)label);
        if(dc){SIZE size={0};if(GetTextExtentPoint32W(dc,label,(int)wcslen(label),&size)&&size.cx>extent)extent=size.cx;}
    }
    if(old)SelectObject(dc,old);if(dc)ReleaseDC(blacklistList,dc);
    SendMessageW(blacklistList,LB_SETHORIZONTALEXTENT,extent+S(18),0);
}
static BOOL changeBlacklist(int remove,LPCWSTR add){
    WCHAR (*old)[2048]=HeapAlloc(GetProcessHeap(),0,sizeof(blacklist));if(!old)return FALSE;
    memcpy(old,blacklist,sizeof(blacklist));int count=blacklistCount;
    AcquireSRWLockExclusive(&blacklistLock);
    if(remove>=0&&remove<blacklistCount){memmove(blacklist+remove,blacklist+remove+1,(blacklistCount-remove-1)*sizeof(blacklist[0]));blacklistCount--;}
    else if(add&&blacklistCount<32){lstrcpynW(blacklist[blacklistCount++],add,2048);}
    ReleaseSRWLockExclusive(&blacklistLock);BOOL ok=saveConfig();
    if(!ok){AcquireSRWLockExclusive(&blacklistLock);memcpy(blacklist,old,sizeof(blacklist));blacklistCount=count;ReleaseSRWLockExclusive(&blacklistLock);}
    HeapFree(GetProcessHeap(),0,old);refreshBlacklist();return ok;
}
static void commitBlacklistCandidate(void){
    if(!blacklistCandidate[0])return;
    if(blacklistMatches(blacklistCandidate,blacklistCandidate)){setBlacklistStatus(L"该应用已在黑名单中，无需重复添加。");EnableWindow(blacklistAddButton,FALSE);return;}
    if(blacklistCount>=32){error(activeDialog,L"黑名单已满，请先解除不需要的项目。");return;}
    if(changeBlacklist(-1,blacklistCandidate)){
        WCHAR text[2300];swprintf(text,2300,L"已添加并保存：%ls\r\n%ls",blacklistCandidate,uuProcess(blacklistCandidate)?L"UU查看端内中键转发；被控端桌面正常唤醒。":L"该应用内不再中键唤醒。可在下方列表解除。");
        setBlacklistStatus(text);EnableWindow(blacklistAddButton,FALSE);
        SendMessageW(blacklistList,LB_SETCURSEL,blacklistCount-1,0);
    }else{setBlacklistStatus(L"保存失败，尚未生效。请点击“重试保存”。");EnableWindow(blacklistAddButton,TRUE);}
}
static void blacklistRecognized(BOOL ok,LPCWSTR path,LPCWSTR target,LPCWSTR problem){
    blacklistPicking=FALSE;ShowWindow(activeDialog,SW_SHOW);SetForegroundWindow(activeDialog);
    if(ok){lstrcpynW(blacklistCandidate,!_wcsnicmp(target,L"shell:AppsFolder\\",17)?target+17:path,2048);commitBlacklistCandidate();}
    else if(problem&&problem[0])setBlacklistStatus(problem);
}
static LRESULT CALLBACK BlacklistProc(HWND w,UINT message,WPARAM wp,LPARAM lp){
    LRESULT color=dialogColors(message,wp);if(color)return color;
    if(message==WM_DRAWITEM&&drawModernButton((DRAWITEMSTRUCT*)lp))return TRUE;
    if(message==WM_DRAWITEM&&drawDialogStatus((DRAWITEMSTRUCT*)lp))return TRUE;
    if(message==WM_NCHITTEST){POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(w,&p);if(p.y<S(GLASS_HEADER_HEIGHT)&&p.x<S(390))return HTCAPTION;return HTCLIENT;}
    if(message==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);
        paintDialogHeader(dc,r,L"中键唤醒黑名单",L"LITELAUNCH  /  MIDDLE CLICK");
        paintDialogField(dc,24,215,592,195,GetFocus()==blacklistList);
        EndPaint(w,&ps);return 0;}
    if(message==WM_COMMAND){
        int id=LOWORD(wp);
        if(id==101&&(HIWORD(wp)==LBN_SETFOCUS||HIWORD(wp)==LBN_KILLFOCUS)){InvalidateRect(w,NULL,FALSE);return 0;}
        if(id==103){if(mouseHook){blacklistCandidate[0]=0;EnableWindow(blacklistAddButton,FALSE);blacklistPicking=TRUE;beginPick();}else error(w,L"鼠标监听不可用，请重启轻启。");return 0;}
        if(id==102){WCHAR path[2048]=L"";if(browse(w,path,FALSE)){const WCHAR *ext=wcsrchr(path,L'.');if(ext&&!_wcsicmp(ext,L".exe"))blacklistRecognized(TRUE,path,path,L"");else error(w,L"请选择EXE程序文件。");}return 0;}
        if(id==104){commitBlacklistCandidate();return 0;}
        if(id==105){int i=(int)SendMessageW(blacklistList,LB_GETCURSEL,0,0);if(i!=LB_ERR&&changeBlacklist(i,NULL))setBlacklistStatus(L"已解除，该应用可再次使用中键唤醒。");return 0;}
        if(id==IDCANCEL||id==IDOK){DestroyWindow(w);return 0;}
    }
    if(message==WM_CLOSE){DestroyWindow(w);return 0;}return DefWindowProcW(w,message,wp,lp);
}
static void editBlacklist(void){
    WNDCLASSW wc={0};wc.lpfnWndProc=BlacklistProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=dialogBackground;wc.lpszClassName=L"LiteLaunch.Blacklist";RegisterClassW(&wc);
    editing=TRUE;blacklistCandidate[0]=0;blacklistPicking=FALSE;ShowWindow(panel,SW_HIDE);
    POINT point;GetCursorPos(&point);MONITORINFO mi={0};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&mi);
    int width=S(640),height=S(480),x=point.x-width/2,y=point.y-40;if(x+width>mi.rcWork.right)x=mi.rcWork.right-width;if(y+height>mi.rcWork.bottom)y=mi.rcWork.bottom-height;if(x<mi.rcWork.left)x=mi.rcWork.left;if(y<mi.rcWork.top)y=mi.rcWork.top;
    HWND w=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"中键唤醒黑名单",WS_POPUP|WS_CLIPCHILDREN,x,y,width,height,NULL,NULL,instance,NULL);
    if(!w){editing=FALSE;showPanel();return;}activeDialog=w;
    roundDialogWindow(w,18);
    HWND label=CreateWindowW(L"STATIC",L"保留应用原有中键操作；UU查看端转发，被控端桌面可唤醒。",WS_CHILD|WS_VISIBLE,S(24),S(67),S(592),S(20),w,NULL,instance,NULL);
    blacklistStatus=CreateWindowExW(0,L"STATIC",L"尚未选择应用。点击“选取窗口”，再点目标软件。",WS_CHILD|WS_VISIBLE|SS_OWNERDRAW,S(24),S(145),S(592),S(56),w,(HMENU)106,instance,NULL);
    blacklistList=CreateWindowExW(0,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,S(25),S(216),S(590),S(192),w,(HMENU)101,instance,NULL);roundFieldControl(blacklistList);
    HWND controls[]={label,blacklistStatus,blacklistList};for(int i=0;i<3;i++)SendMessageW(controls[i],WM_SETFONT,(WPARAM)font,TRUE);
    HWND close=CreateWindowW(L"BUTTON",L"×",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,S(592),S(16),S(24),S(32),w,(HMENU)(INT_PTR)IDCANCEL,instance,NULL);SendMessageW(close,WM_SETFONT,(WPARAM)font,TRUE);
    const WCHAR *labels[]={L"选取窗口",L"选择EXE…",L"重试保存",L"解除选中项",L"完成"};int ids[]={103,102,104,105,IDCANCEL},xs[]={24,168,312,24,512},ys[]={98,98,98,430,430};
    for(int i=0;i<5;i++){HWND b=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,S(xs[i]),S(ys[i]),S(i==4?104:(i==3?144:132)),S(36),w,(HMENU)(INT_PTR)ids[i],instance,NULL);SendMessageW(b,WM_SETFONT,(WPARAM)font,TRUE);if(ids[i]==104)blacklistAddButton=b;}
    EnableWindow(blacklistAddButton,FALSE);refreshBlacklist();ShowWindow(w,SW_SHOW);SetForegroundWindow(w);MSG msg;
    while(IsWindow(w)){int got=GetMessageW(&msg,NULL,0,0);if(got<=0){if(!got)PostQuitMessage(0);break;}if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){DestroyWindow(w);continue;}if(!IsDialogMessageW(w,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    activeDialog=NULL;editing=FALSE;blacklistPicking=FALSE;picking=FALSE;UnregisterHotKey(panel,2);stopPickerVisual();showPanel();
}
