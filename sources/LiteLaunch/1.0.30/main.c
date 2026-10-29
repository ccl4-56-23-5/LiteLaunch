#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <propkey.h>
#include <appmodel.h>
#include <tlhelp32.h>
#include <oleacc.h>
#include <gdiplus.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "glass.h"
#include "ui_theme.h"
#include "startup.h"
#include "update.h"
#include "runtime_log.h"
#include "plugins.h"

#define COUNT 128
#define FUNCTION_COUNT 4
#define APP_CELL_START (FUNCTION_COUNT+1)
#define CELL_COUNT (((COUNT+APP_CELL_START+3)/4)*4)
#define WM_TRAY (WM_APP+1)
#define WM_SHOWPANEL (WM_APP+2)
#define WM_PICK (WM_APP+3)
#define WM_CANCELPICK (WM_APP+4)
#define WM_OPENENTRY (WM_APP+5)
#define WM_PICKHOVER (WM_APP+6)
#define WM_EXITAPP (WM_APP+7)
#define WM_EVERACTION (WM_APP+9)
#define EVER_COPYDATA 0x45564552
static WCHAR everPendingCommand[8192];
static HWND everPendingSource;
static BOOL everPendingReady=FALSE;
#define WM_MIDDLE_DIAGNOSTIC (WM_APP+8)
#define TRAY_TIMER 3
#define CLASSNAME L"LiteLaunch.Native.Panel.v1"
typedef struct { WCHAR name[128], target[2048], args[2048], icon[1024]; HICON image; void *visual; } Entry;
static Entry entries[COUNT];
static HWND panel, editor, activeDialog, fields[4],sourceWindow;
static HINSTANCE instance;
static HFONT font, titleFont, symbolFont, metaFont;
static NOTIFYICONDATAW tray;
static BOOL trayRegistered;
static WCHAR config[MAX_PATH], configTemp[MAX_PATH], configBackup[MAX_PATH], configBackupTemp[MAX_PATH];
static WCHAR configInitial[MAX_PATH], configInitialTemp[MAX_PATH], legacyConfig[MAX_PATH];
static WCHAR configFailure[512];
static BOOL pinned, editing, saved, tracking, liveCapture, positionSaved;
static POINT savedPosition;
static int panelWidth=448,displayRows=4,visibleRows=4,scrollRow;
static int themeIndex=0;
static int systemDpi=96,scalePercent=100;
static ULONGLONG lastCaptureTick;
static void updateRows(void);
static BOOL dragging, iconDragging;
static int pressedIndex=-1;
static POINT pressPoint;
static POINT dragStart, windowStart;
static int hover=-1, selection=-1, editIndex, dpi=96;
static UINT taskbarCreated;
static HHOOK mouseHook;
static HANDLE hookThread,hookReady;
static DWORD hookThreadId;
static volatile LONG picking;
static BOOL swallowLeft, swallowRight;
/* Physical and injected clicks can interleave; latch each down/up pair. */
static BOOL middleDown[2], swallowMiddle[2];
static WCHAR blacklist[32][2048];
static int blacklistCount;
static SRWLOCK blacklistLock=SRWLOCK_INIT;
static HWND pickTip;
static BOOL blacklistPicking;
static void stopPickerVisual(void);
static LRESULT dialogColors(UINT msg,WPARAM wp);
static void roundFieldControl(HWND control);
static LARGE_INTEGER pickStart;
static double lastPickMs;
static WCHAR editorTitle[96],editorStatus[128];
static void finishPick(HWND selected);
static void beginPick(void);
static void menu(int index,POINT p);
static int S(int n) { return MulDiv(n,dpi,96); }
static int appIndexForCell(int cell){return cell>=APP_CELL_START&&cell<COUNT+APP_CELL_START?cell-APP_CELL_START:-1;}
static BOOL selectableCell(int cell){return (cell>=0&&cell<=FUNCTION_COUNT)||appIndexForCell(cell)>=0;}
static void rememberSourceWindow(HWND window){DWORD pid=0;if(window&&IsWindow(window)){GetWindowThreadProcessId(window,&pid);if(pid&&pid!=GetCurrentProcessId())sourceWindow=window;}}
static void error(HWND owner, LPCWSTR message) {
#ifdef LITELAUNCH_CONFIG_TEST
    (void)owner;(void)message;
#else
    BOOL was=editing;editing=TRUE;MessageBoxW(owner,message,L"轻启 LiteLaunch",MB_OK|MB_ICONERROR);editing=was;
#endif
}
static void section(int i,WCHAR *s) { swprintf(s,32,L"Item%d",i); }
static BOOL ensureTray(BOOL rebuild) {
    if(!tray.hWnd)return FALSE;
    BOOL ok;
    if(rebuild||!trayRegistered){ok=Shell_NotifyIconW(NIM_ADD,&tray);if(!ok)ok=Shell_NotifyIconW(NIM_MODIFY,&tray);}
    else {ok=Shell_NotifyIconW(NIM_MODIFY,&tray);if(!ok)ok=Shell_NotifyIconW(NIM_ADD,&tray);}
    if(ok!=trayRegistered||!ok||rebuild)runtimeLog(L"tray registered=%d rebuild=%d",ok,rebuild);
    trayRegistered=ok;
    if(!SetTimer(tray.hWnd,TRAY_TIMER,ok?30000:2000,NULL))runtimeLog(L"tray retry timer failed error=%lu",GetLastError());
    return ok;
}
static void normalize(WCHAR *s) {
    /* Accept the common copied Markdown escape before a package-family underscore. */
    if (!_wcsnicmp(s,L"shell:AppsFolder\\",17)) {
        WCHAR *p=wcsstr(s+17,L"\\_");
        if (p) memmove(p,p+1,(wcslen(p+1)+1)*sizeof(WCHAR));
    }
}
static LPCWSTR fileName(LPCWSTR path) {
    LPCWSTR a=wcsrchr(path,L'\\');LPCWSTR b=wcsrchr(path,L'/');if(b&&(!a||b>a))a=b;return a?a+1:path;
}
static BOOL isClashVergePath(LPCWSTR path) { return !_wcsicmp(fileName(path),L"clash-verge.exe"); }
static BOOL isWebViewPath(LPCWSTR path) { return !_wcsicmp(fileName(path),L"msedgewebview2.exe"); }
static BOOL processPath(DWORD pid,WCHAR *path,DWORD capacity) {
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return FALSE;
    DWORD len=capacity;BOOL ok=QueryFullProcessImageNameW(process,0,path,&len);CloseHandle(process);return ok;
}
typedef struct { LPCWSTR target;HWND window; } RunningTargetWindow;
static BOOL CALLBACK findHiddenTargetWindowProc(HWND window,LPARAM data){
    RunningTargetWindow *match=(RunningTargetWindow*)data;
    if(GetWindow(window,GW_OWNER)||(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)||GetWindowTextLengthW(window)==0)return TRUE;
    if(IsWindowVisible(window)&&!IsIconic(window))return TRUE;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    WCHAR path[2048]=L"";
    if(pid&&processPath(pid,path,2048)&&!_wcsicmp(path,match->target)){
        match->window=window;return FALSE;
    }
    return TRUE;
}
static HWND hiddenTargetWindow(LPCWSTR target){
    LPCWSTR extension=wcsrchr(target,L'.');if(!extension||_wcsicmp(extension,L".exe"))return NULL;
    RunningTargetWindow match={target,NULL};EnumWindows(findHiddenTargetWindowProc,(LPARAM)&match);
    return match.window;
}
static BOOL revealTargetWindow(HWND existing,int slot){
    if(!existing||!IsWindow(existing)||(IsWindowVisible(existing)&&!IsIconic(existing)))return FALSE;
    ShowWindow(existing,SW_RESTORE);
    BOOL focused=SetForegroundWindow(existing),visible=IsWindowVisible(existing);
    runtimeLog(L"existing application window restored slot=%d visible=%d focused=%d",slot,visible,focused);
    return visible;
}
static DWORD parentProcessId(DWORD pid) {
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W entry={0};entry.dwSize=sizeof(entry);DWORD parent=0;
    if(Process32FirstW(snap,&entry))do{if(entry.th32ProcessID==pid){parent=entry.th32ParentProcessID;break;}}while(Process32NextW(snap,&entry));
    CloseHandle(snap);return parent;
}
static DWORD findClashParent(DWORD pid,WCHAR *path,DWORD capacity) {
    for(int depth=0;depth<8&&pid;depth++){
        DWORD parent=parentProcessId(pid);if(!parent||parent==pid)break;
        WCHAR candidate[2048]=L"";if(processPath(parent,candidate,2048)&&isClashVergePath(candidate)){lstrcpynW(path,candidate,capacity);return parent;}
        pid=parent;
    }return 0;
}
static void loadIcon(Entry *e) {
    if(e->image){DestroyIcon(e->image);e->image=NULL;}
    if(e->visual)glassFreeIcon(e->visual);
    e->visual=e->target[0]?glassLoadIcon(e->icon,e->name,e->target,dpi):NULL;
}
static BOOL fileExists(LPCWSTR path) { return GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES; }
static BOOL setConfigPaths(LPCWSTR dataDirectory,LPCWSTR executableDirectory) {
    if(wcslen(dataDirectory)+wcslen(L"\\LiteLaunch.initial.ini.tmp")>=MAX_PATH ||
       wcslen(executableDirectory)+wcslen(L"\\LiteLaunch.ini")>=MAX_PATH)return FALSE;
    swprintf(config,MAX_PATH,L"%ls\\LiteLaunch.ini",dataDirectory);
    swprintf(configTemp,MAX_PATH,L"%ls.tmp",config);
    swprintf(configBackup,MAX_PATH,L"%ls.bak",config);
    swprintf(configBackupTemp,MAX_PATH,L"%ls.tmp",configBackup);
    swprintf(configInitial,MAX_PATH,L"%ls\\LiteLaunch.initial.ini",dataDirectory);
    swprintf(configInitialTemp,MAX_PATH,L"%ls.tmp",configInitial);
    swprintf(legacyConfig,MAX_PATH,L"%ls\\LiteLaunch.ini",executableDirectory);
    return TRUE;
}
static BOOL initConfigPaths(void) {
    WCHAR appData[MAX_PATH],directory[MAX_PATH],exe[MAX_PATH];
    if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,appData)))return FALSE;
    if(wcslen(appData)+wcslen(L"\\LiteLaunch")>=MAX_PATH)return FALSE;
    swprintf(directory,MAX_PATH,L"%ls\\LiteLaunch",appData);
    if(!CreateDirectoryW(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return FALSE;
    DWORD length=GetModuleFileNameW(NULL,exe,MAX_PATH);
    if(!length||length>=MAX_PATH)return FALSE;
    WCHAR *slash=wcsrchr(exe,L'\\');if(!slash)return FALSE;*slash=0;
    return setConfigPaths(directory,exe);
}
/* Accept an intentionally empty panel, but never treat a truncated or unreadable INI as empty. */
static BOOL validConfig(LPCWSTR path) {
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE)return FALSE;
    LARGE_INTEGER size={0};WORD bom=0;DWORD read=0;
    BOOL bytesOk=GetFileSizeEx(f,&size)&&size.QuadPart>=2&&size.QuadPart<=4*1024*1024&&!(size.QuadPart&1)&&
        ReadFile(f,&bom,2,&read,NULL)&&read==2&&bom==0xfeff;
    CloseHandle(f);if(!bytesOk)return FALSE;
    WCHAR text[32],*end=NULL;
    if(!GetPrivateProfileStringW(L"Panel",L"StoredSlots",L"",text,32,path))return FALSE;
    long count=wcstol(text,&end,10);
    if(!end||*end||count<12||count>COUNT)return FALSE;
    static const WCHAR *keys[]={L"Name",L"Target",L"Arguments",L"Icon"};
    for(int i=0;i<count;i++) {
        WCHAR sec[32],value[4];section(i,sec);
        for(int k=0;k<4;k++) {
            GetPrivateProfileStringW(sec,keys[k],L"\xe000\xe001",value,4,path);
            if(!wcscmp(value,L"\xe000\xe001"))return FALSE;
        }
    }
    if(!GetPrivateProfileStringW(L"Blacklist",L"Count",L"",text,32,path))return FALSE;
    long blacklistItems=wcstol(text,&end,10);
    if(!end||*end||blacklistItems<0||blacklistItems>32)return FALSE;
    for(int i=0;i<blacklistItems;i++){
        WCHAR key[32],value[4];swprintf(key,32,L"App%d",i);
        GetPrivateProfileStringW(L"Blacklist",key,L"\xe000\xe001",value,4,path);
        if(!wcscmp(value,L"\xe000\xe001"))return FALSE;
    }
    return TRUE;
}
static BOOL atomicConfigCopy(LPCWSTR source,LPCWSTR destination,LPCWSTR temporary) {
    if(!CopyFileW(source,temporary,FALSE))return FALSE;
    if(!validConfig(temporary))return FALSE;
    return MoveFileExW(temporary,destination,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
static int configuredTargets(LPCWSTR path) {
    int count=GetPrivateProfileIntW(L"Panel",L"StoredSlots",0,path),used=0;
    if(count<0||count>COUNT)return 0;
    for(int i=0;i<count;i++){
        WCHAR sec[32],target[2];section(i,sec);
        if(GetPrivateProfileStringW(sec,L"Target",L"",target,2,path))used++;
    }
    return used;
}
/* A nontrivial initial list is a recovery anchor; ordinary saves never replace it. */
static BOOL ensureInitialSnapshot(LPCWSTR source) {
    if(fileExists(configInitial))return validConfig(configInitial);
    if(configuredTargets(source)<2)return TRUE;
    return atomicConfigCopy(source,configInitial,configInitialTemp);
}
static BOOL configProblem(LPCWSTR message) { lstrcpynW(configFailure,message,512);return FALSE; }
static BOOL prepareConfig(void) {
    configFailure[0]=0;
    if(fileExists(config)) {
        if(!validConfig(config)) {
            LPCWSTR recovery=validConfig(configBackup)?configBackup:(validConfig(configInitial)?configInitial:NULL);
            if(!recovery||!atomicConfigCopy(recovery,config,configTemp)) {
                return configProblem(L"LiteLaunch 主配置损坏，备份和初始快照均不可用或无法恢复。已停止启动，避免用默认列表覆盖原配置。请检查 LocalAppData\\LiteLaunch 中的 INI。");
            }
        }
    } else if(fileExists(configBackup)||fileExists(configInitial)) {
        LPCWSTR recovery=validConfig(configBackup)?configBackup:(validConfig(configInitial)?configInitial:NULL);
        if(!recovery||!atomicConfigCopy(recovery,config,configTemp)) {
            return configProblem(L"LiteLaunch 主配置缺失，备份和初始快照均不可用或无法恢复。已停止启动，避免覆盖残存配置。");
        }
    } else if(_wcsicmp(config,legacyConfig)&&fileExists(legacyConfig)) {
        if(!validConfig(legacyConfig)||!atomicConfigCopy(legacyConfig,config,configTemp)) {
            return configProblem(L"EXE 邻侧的旧 LiteLaunch.ini 无法校验或迁移。已停止启动；请保留旧文件并检查路径与读写权限。");
        }
    }
    if(fileExists(config)&&!validConfig(config))return configProblem(L"LiteLaunch 配置迁移后未通过校验。已停止启动；请检查 INI 和 BAK。");
    if(fileExists(config)&&!validConfig(configBackup)&&!atomicConfigCopy(config,configBackup,configBackupTemp)) {
        return configProblem(L"LiteLaunch 配置已找到，但无法创建有效备份。已停止启动以保护应用列表。请检查 LocalAppData\\LiteLaunch 的写入权限。");
    }
    if(fileExists(config)&&!ensureInitialSnapshot(config)) {
        return configProblem(L"LiteLaunch 初始恢复快照损坏或无法创建。已停止启动并保留原文件；请检查 LocalAppData\\LiteLaunch 中的 LiteLaunch.initial.ini 与写入权限。");
    }
    return TRUE;
}
static BOOL saveConfig(void) {
    HANDLE f=CreateFileW(configTemp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE) { error(panel,L"无法写入配置。请检查 %LOCALAPPDATA%\\LiteLaunch 的写入权限。"); return FALSE; }
    WORD bom=0xfeff; DWORD written;
    BOOL ok=WriteFile(f,&bom,2,&written,NULL) && written==2;
    CloseHandle(f);
    WCHAR sec[32];
    int storedCount=COUNT;while(storedCount>12&&!entries[storedCount-1].target[0]&&!entries[storedCount-1].name[0])storedCount--;
    WCHAR storedText[32];swprintf(storedText,32,L"%d",storedCount);ok=WritePrivateProfileStringW(L"Panel",L"StoredSlots",storedText,configTemp)&&ok;
    for(int i=0;i<storedCount;i++) {
        section(i,sec);
        ok=WritePrivateProfileStringW(sec,L"Name",entries[i].name,configTemp)&&ok;
        ok=WritePrivateProfileStringW(sec,L"Target",entries[i].target,configTemp)&&ok;
        ok=WritePrivateProfileStringW(sec,L"Arguments",entries[i].args,configTemp)&&ok;
        ok=WritePrivateProfileStringW(sec,L"Icon",entries[i].icon,configTemp)&&ok;
    }
    WCHAR value[32];swprintf(value,32,L"%d",panelWidth);ok=WritePrivateProfileStringW(L"Panel",L"Width",value,configTemp)&&ok;
    swprintf(value,32,L"%d",themeIndex);ok=WritePrivateProfileStringW(L"Panel",L"Theme",value,configTemp)&&ok;
    swprintf(value,32,L"%d",scalePercent);ok=WritePrivateProfileStringW(L"Panel",L"ScalePercent",value,configTemp)&&ok;
    if(positionSaved){swprintf(value,32,L"%ld",savedPosition.x);ok=WritePrivateProfileStringW(L"Panel",L"X",value,configTemp)&&ok;swprintf(value,32,L"%ld",savedPosition.y);ok=WritePrivateProfileStringW(L"Panel",L"Y",value,configTemp)&&ok;}
    WCHAR countText[32];swprintf(countText,32,L"%d",blacklistCount);ok=WritePrivateProfileStringW(L"Blacklist",L"Count",countText,configTemp)&&ok;
    for(int i=0;i<blacklistCount;i++){WCHAR key[32];swprintf(key,32,L"App%d",i);ok=WritePrivateProfileStringW(L"Blacklist",key,blacklist[i],configTemp)&&ok;}
    WritePrivateProfileStringW(NULL,NULL,NULL,configTemp);
    if(!ok||!validConfig(configTemp)) {
        error(panel,L"生成的配置文件未通过校验，原配置保持不变。");return FALSE;
    }
    if(!ensureInitialSnapshot(configTemp)) {
        error(panel,L"初始恢复快照损坏或无法创建，保存已取消；原文件保持不变。");return FALSE;
    }
    if(fileExists(config)) {
        if(!validConfig(config)||!atomicConfigCopy(config,configBackup,configBackupTemp)) {
            error(panel,L"原配置或备份写入有误，保存已取消，原配置保持不变。");return FALSE;
        }
    } else {
        if(fileExists(configBackup)||!atomicConfigCopy(configTemp,configBackup,configBackupTemp)) {
            error(panel,L"备份不可用，保存已取消，避免覆盖原有配置。");return FALSE;
        }
    }
    if(!MoveFileExW(configTemp,config,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        error(panel,L"保存配置失败，原配置保持不变。"); return FALSE;
    }
    return TRUE;
}
static BOOL loadConfig(void) {
    if(!prepareConfig())return FALSE;
    BOOL migrate=FALSE;
    if(GetFileAttributesW(config)==INVALID_FILE_ATTRIBUTES) {
        lstrcpyW(entries[0].name,L"Codex");
        lstrcpyW(entries[0].target,L"shell:AppsFolder\\OpenAI.Codex_2p2nqsd0c76g0!App");
    } else {
        WCHAR sec[32];
        for(int i=0;i<COUNT;i++) {
            section(i,sec);
            GetPrivateProfileStringW(sec,L"Name",L"",entries[i].name,128,config);
            GetPrivateProfileStringW(sec,L"Target",L"",entries[i].target,2048,config);
            GetPrivateProfileStringW(sec,L"Arguments",L"",entries[i].args,2048,config);
            GetPrivateProfileStringW(sec,L"Icon",L"",entries[i].icon,1024,config);
            normalize(entries[i].target);
            if(isClashVergePath(entries[i].target)){
                if(_wcsicmp(entries[i].name,L"Clash Verge")){lstrcpyW(entries[i].name,L"Clash Verge");migrate=TRUE;}
                if(isWebViewPath(entries[i].icon)){lstrcpyW(entries[i].icon,entries[i].target);migrate=TRUE;}
            }
        }
    }
    panelWidth=GetPrivateProfileIntW(L"Panel",L"Width",448,config);if(panelWidth<400||panelWidth>500){panelWidth=448;if(GetFileAttributesW(config)!=INVALID_FILE_ATTRIBUTES)migrate=TRUE;}
    themeIndex=GetPrivateProfileIntW(L"Panel",L"Theme",0,config);if(themeIndex<0||themeIndex>9)themeIndex=0;glassTheme(themeIndex);
    scalePercent=GetPrivateProfileIntW(L"Panel",L"ScalePercent",100,config);
    if(scalePercent<60||scalePercent>200)scalePercent=100;
    dpi=MulDiv(systemDpi,scalePercent,100);
    WCHAR pos[32];GetPrivateProfileStringW(L"Panel",L"X",L"",pos,32,config);positionSaved=pos[0]!=0;
    savedPosition.x=GetPrivateProfileIntW(L"Panel",L"X",0,config);savedPosition.y=GetPrivateProfileIntW(L"Panel",L"Y",0,config);
    blacklistCount=GetPrivateProfileIntW(L"Blacklist",L"Count",0,config);if(blacklistCount<0||blacklistCount>32)blacklistCount=0;
    for(int i=0;i<blacklistCount;i++){WCHAR key[32];swprintf(key,32,L"App%d",i);GetPrivateProfileStringW(L"Blacklist",key,L"",blacklist[i],2048,config);}
    for(int i=0;i<COUNT;i++) loadIcon(&entries[i]);
    if(migrate)saveConfig();
    return TRUE;
}
static int neededRows(void){int last=-1;for(int i=0;i<COUNT;i++)if(entries[i].target[0])last=i;int used=last+2,rows=(used+3)/4;if(used%4==0&&used<COUNT+1)rows++;if(rows<3)rows=3;return rows;}
static void updateRows(void){
    displayRows=neededRows()+1;MONITORINFO mi={0};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromWindow(panel,MONITOR_DEFAULTTONEAREST),&mi);
    visibleRows=(mi.rcWork.bottom-mi.rcWork.top-S(GLASS_GRID_TOP+GLASS_FOOTER_HEIGHT))/S(GLASS_ROW_PITCH);if(visibleRows<2)visibleRows=2;if(visibleRows>displayRows)visibleRows=displayRows;
    if(scrollRow>displayRows-visibleRows)scrollRow=displayRows-visibleRows;if(scrollRow<0)scrollRow=0;
    glassRows(displayRows*4,scrollRow);
    if(panel){int height=S(GLASS_GRID_TOP+visibleRows*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT),width=S(panelWidth);RECT wr;GetWindowRect(panel,&wr);int x=wr.left,y=wr.top;
        if(x+width>mi.rcWork.right)x=mi.rcWork.right-width;if(x<mi.rcWork.left)x=mi.rcWork.left;
        if(y+height>mi.rcWork.bottom)y=mi.rcWork.bottom-height;if(y<mi.rcWork.top)y=mi.rcWork.top;
        if(wr.right-wr.left!=width||wr.bottom-wr.top!=height||x!=wr.left||y!=wr.top)SetWindowPos(panel,NULL,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);}
}
static void updateFonts(void){
    if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);
    if(symbolFont)DeleteObject(symbolFont);if(metaFont)DeleteObject(metaFont);
    font=CreateFontW(-S(14),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    titleFont=CreateFontW(-S(18),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    symbolFont=CreateFontW(-S(22),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    metaFont=CreateFontW(-S(11),0,0,0,FW_MEDIUM,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
}
static void applyScale(int percent){
    if(percent<60||percent>200)return;
    int old=scalePercent;scalePercent=percent;
    if(!saveConfig()){scalePercent=old;return;}
    dpi=MulDiv(systemDpi,scalePercent,100);updateFonts();
    for(int i=0;i<COUNT;i++)if(entries[i].target[0])loadIcon(&entries[i]);
    updateRows();InvalidateRect(panel,NULL,FALSE);
}
/* A remote desktop must be able to capture the launch panel.
   Capture the background only while hidden, to avoid sampling our own surface. */
static BOOL configurePanelCapture(void){
    liveCapture=FALSE;
    BOOL ok=SetWindowDisplayAffinity(panel,WDA_NONE);DWORD affinity=0xFFFFFFFF;
    BOOL read=GetWindowDisplayAffinity(panel,&affinity);
    runtimeLog(L"panel capture policy unrestricted=%d affinity=0x%08lX live_background=0",ok&&read&&affinity==WDA_NONE,affinity);
    return ok&&read&&affinity==WDA_NONE;
}
static void showPanel(void) {
    if(!IsWindowVisible(panel))rememberSourceWindow(GetForegroundWindow());
    POINT p; GetCursorPos(&p);
    MONITORINFO mi={0};mi.cbSize=sizeof(mi);
    GetMonitorInfoW(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),&mi);
    scrollRow=0;updateRows();int w=S(panelWidth),h=S(GLASS_GRID_TOP+visibleRows*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT);
    int x=p.x-w/2,y=p.y-S(20);
    hover=selection=-1;pressedIndex=-1;iconDragging=FALSE;glassResetSelection();
    if(x<mi.rcWork.left)x=mi.rcWork.left;
    if(y<mi.rcWork.top)y=mi.rcWork.top;
    if(x+w>mi.rcWork.right)x=mi.rcWork.right-w;
    if(y+h>mi.rcWork.bottom)y=mi.rcWork.bottom-h;
    if(!IsWindowVisible(panel))glassCapture(x,y,w,h);
    SetWindowPos(panel,HWND_TOPMOST,x,y,w,h,SWP_SHOWWINDOW);
    // SWP_SHOWWINDOW is not guaranteed to send WM_SHOWWINDOW. Start explicitly.
    lastCaptureTick=0;SetTimer(panel,1,16,NULL);
    InvalidateRect(panel,NULL,FALSE);
    BOOL activated=SetForegroundWindow(panel);SetFocus(panel);
    DWORD affinity=0xFFFFFFFF;GetWindowDisplayAffinity(panel,&affinity);
    runtimeLog(L"panel show foreground=%d visible=%d affinity=0x%08lX",activated,IsWindowVisible(panel),affinity);
}
static int cellAt(int x,int y){
    float xx=x*96.f/dpi,yy=y*96.f/dpi,pitch=(panelWidth-24.f)/4;
    if(xx<12||yy<GLASS_GRID_TOP||xx>=panelWidth-12||yy>=GLASS_GRID_TOP+visibleRows*GLASS_ROW_PITCH)return -1;
    int row=(int)((yy-GLASS_GRID_TOP)/GLASS_ROW_PITCH),column=(int)((xx-12)/pitch);
    int cell=(row==0?0:row+scrollRow)*4+column;return cell<COUNT+APP_CELL_START?cell:-1;
}
static HWND control(LPCWSTR cls,LPCWSTR text,DWORD style,int x,int y,int w,int h,int id) {
    BOOL isEdit=!wcscmp(cls,L"EDIT");
    /* The outer 36px field is painted by the dialog. Keep the native single-line
       EDIT centered inside it so both glyphs and the system caret have padding.
       The horizontal inset never drops below 3px: at 75% scaling S(1) collapses
       to a hairline and the white edit then reads as breaking out of its frame. */
    int horizontalInset=isEdit?((S(3)>3)?S(3):3):0;
    int verticalInset=isEdit?((S(8)>4)?S(8):4):0;
    HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,
        S(x)+horizontalInset,S(y)+verticalInset,S(w)-horizontalInset*2,
        S(h)-verticalInset*2,editor,(HMENU)(INT_PTR)id,instance,NULL);
    SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);
    if(isEdit)SendMessageW(c,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(S(12),S(12)));
    /* The painted field is rounded; a square EDIT would paint over its corners. */
    if(isEdit)roundFieldControl(c);
    return c;
}
static BOOL browse(HWND owner,WCHAR *path,BOOL icon) {
    OPENFILENAMEW of={0};of.lStructSize=sizeof(of);
    of.hwndOwner=owner; of.lpstrFile=path; of.nMaxFile=2048;
    of.lpstrFilter=icon?L"SVG、官方图片或程序\0*.svg;*.png;*.ico;*.exe;*.dll\0所有文件\0*.*\0":L"应用与快捷方式\0*.exe;*.lnk;*.url;*.bat;*.cmd\0所有文件\0*.*\0";
    of.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR|OFN_DONTADDTORECENT|OFN_NODEREFERENCELINKS;
    return GetOpenFileNameW(&of);
}
static void nameFromPath(LPCWSTR path,WCHAR *out) {
    LPCWSTR b=wcsrchr(path,L'\\'); b=b?b+1:path;
    lstrcpynW(out,b,128); WCHAR *dot=wcsrchr(out,L'.'); if(dot)*dot=0;
}
/* Resolve a shell shortcut into the same application fields used by window picking. */
static BOOL profileFromPath(LPCWSTR source,Entry *entry) {
    ZeroMemory(entry,sizeof(*entry));
    nameFromPath(source,entry->name);
    LPCWSTR ext=wcsrchr(source,L'.');
    if(!ext||_wcsicmp(ext,L".lnk")){
        lstrcpynW(entry->target,source,2048);return TRUE;
    }
    IShellLinkW *link=NULL;IPersistFile *file=NULL;BOOL ok=FALSE;
    HRESULT hr=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);
    if(FAILED(hr))goto done;
    hr=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&file);
    if(FAILED(hr)||FAILED(file->lpVtbl->Load(file,source,STGM_READ)))goto done;
    WIN32_FIND_DATAW data={0};
    IShellLinkW_GetPath(link,entry->target,2048,&data,SLGP_RAWPATH);
    IShellLinkW_GetArguments(link,entry->args,2048);
    int iconIndex=0;IShellLinkW_GetIconLocation(link,entry->icon,1024,&iconIndex);
    if(!entry->target[0]){
        IPropertyStore *properties=NULL;
        if(SUCCEEDED(IShellLinkW_QueryInterface(link,&IID_IPropertyStore,(void**)&properties))){
            PROPVARIANT appId={0};
            if(SUCCEEDED(IPropertyStore_GetValue(properties,&PKEY_AppUserModel_ID,&appId))&&appId.vt==VT_LPWSTR&&appId.pwszVal)
                swprintf(entry->target,2048,L"shell:AppsFolder\\%ls",appId.pwszVal);
            PropVariantClear(&appId);IPropertyStore_Release(properties);
        }
    }
    if(!entry->target[0])goto done;
    if(!entry->icon[0]||!fileExists(entry->icon)){
        if(entry->target[0]&&_wcsnicmp(entry->target,L"shell:AppsFolder\\",17))lstrcpynW(entry->icon,entry->target,1024);
        else entry->icon[0]=0;
    }
    ok=TRUE;
done:
    if(file)file->lpVtbl->Release(file);
    if(link)IShellLinkW_Release(link);
    if(!ok)ZeroMemory(entry,sizeof(*entry));
    return ok;
}
#include "picker_style.h"
#include <tlhelp32.h>
#include "blacklist.h"
static LRESULT CALLBACK MouseProc(int code,WPARAM wp,LPARAM lp) {
    if(code==HC_ACTION) {
        MSLLHOOKSTRUCT *m=(MSLLHOOKSTRUCT*)lp;
        if(wp==WM_LBUTTONUP&&swallowLeft){swallowLeft=FALSE;return 1;}
        if(wp==WM_RBUTTONUP&&swallowRight){swallowRight=FALSE;return 1;}
        if(picking&&wp==WM_MOUSEMOVE){HWND target=GetAncestor(WindowFromPoint(m->pt),GA_ROOT);if(target!=lastPickHover){lastPickHover=target;PostMessageW(panel,WM_PICKHOVER,(WPARAM)target,0);}}
        if(picking&&wp==WM_LBUTTONDOWN) {
            HWND hit=WindowFromPoint(m->pt);DWORD pid=0;GetWindowThreadProcessId(hit,&pid);
            if(pid!=GetCurrentProcessId()){picking=FALSE;swallowLeft=TRUE;PostMessageW(panel,WM_PICK,(WPARAM)hit,0);return 1;}
        }
        if(picking&&wp==WM_RBUTTONDOWN){picking=FALSE;swallowRight=TRUE;PostMessageW(panel,WM_CANCELPICK,0,0);return 1;}
        int source=(m->flags&(LLMHF_INJECTED|LLMHF_LOWER_IL_INJECTED))?1:0;
        if(wp==WM_MBUTTONDOWN){
            if(!middleDown[source]){
                int blocked=picking?0:blockedAtPoint(m->pt,m->flags);
                middleDown[source]=TRUE;swallowMiddle[source]=picking||!blocked;
                PostMessageW(panel,WM_MIDDLE_DIAGNOSTIC,swallowMiddle[source],MAKELPARAM(m->flags,blocked));
                if(swallowMiddle[source])PostMessageW(panel,picking?WM_CANCELPICK:WM_HOTKEY,1,0);
            }
            if(swallowMiddle[source])return 1;
        }
        if(wp==WM_MBUTTONUP){BOOL consume=middleDown[source]&&swallowMiddle[source];middleDown[source]=swallowMiddle[source]=FALSE;if(consume)return 1;}
    }
    return CallNextHookEx(mouseHook,code,wp,lp);
}
static DWORD WINAPI HookThreadProc(LPVOID unused) {
    (void)unused;MSG m;PeekMessageW(&m,NULL,WM_USER,WM_USER,PM_NOREMOVE);
    mouseHook=SetWindowsHookExW(WH_MOUSE_LL,MouseProc,instance,0);SetEvent(hookReady);
    while(GetMessageW(&m,NULL,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}
    if(mouseHook)UnhookWindowsHookEx(mouseHook);mouseHook=NULL;return 0;
}
static void beginPick(void) {
    if(!mouseHook){error(editor,L"鼠标监听不可用，请重新启动轻启后重试。");return;}
    ShowWindow(activeDialog,SW_HIDE);ShowWindow(panel,SW_HIDE);
    RegisterHotKey(panel,2,0,VK_ESCAPE);picking=TRUE;startPickerVisual();
}
static BOOL childProcess(HWND w,LPARAM lp) {
    DWORD pid;GetWindowThreadProcessId(w,&pid);DWORD *result=(DWORD*)lp;
    if(pid&&pid!=result[0]){result[1]=pid;return FALSE;}return TRUE;
}
static void finishPick(HWND selected) {
    picking=FALSE;UnregisterHotKey(panel,2);stopPickerVisual();
    QueryPerformanceCounter(&pickStart);
    BOOL ok=FALSE;WCHAR target[2048]=L"",path[2048]=L"",name[128]=L"",problem[256]=L"无法读取窗口所属程序，请尝试手动填写启动目标。";
    if(selected) {
        DWORD selectedPid=0,rootPid=0;GetWindowThreadProcessId(selected,&selectedPid);
        HWND root=GetAncestor(selected,GA_ROOT);if(root)GetWindowThreadProcessId(root,&rootPid);
        DWORD pid=rootPid?rootPid:selectedPid;BOOL gotPath=processPath(pid,path,2048);
        if(!gotPath&&selectedPid&&selectedPid!=pid){pid=selectedPid;gotPath=processPath(pid,path,2048);}
        if(gotPath&&isWebViewPath(path)){
            DWORD owner=findClashParent(pid,path,2048);if(owner)pid=owner;
        }
        if(gotPath) {
            LPCWSTR base=wcsrchr(path,L'\\');base=base?base+1:path;
            if(!_wcsicmp(base,L"ApplicationFrameHost.exe")) {
                DWORD ids[2]={pid,0};EnumChildWindows(root?root:selected,childProcess,(LPARAM)ids);
                if(ids[1]){pid=ids[1];processPath(pid,path,2048);}
            }
            HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
            WCHAR appId[1024];UINT32 size=1024;
            if(process&&GetApplicationUserModelId(process,&size,appId)==ERROR_SUCCESS&&appId[0]) {
                swprintf(target,2048,L"shell:AppsFolder\\%ls",appId);
            }else lstrcpynW(target,path,2048);
            nameFromPath(path,name);
            if(wcsstr(target,L"OpenAI.Codex_"))lstrcpyW(name,L"Codex");
            if(isClashVergePath(path)){lstrcpyW(name,L"Clash Verge");lstrcpyW(target,path);}
            ok=TRUE;
            if(process)CloseHandle(process);
        } else if(GetLastError()==ERROR_ACCESS_DENIED)lstrcpyW(problem,L"此窗口的进程拒绝读取。可手动填写路径，或选择该应用的普通权限窗口。");
    }
    LARGE_INTEGER end,freq;QueryPerformanceCounter(&end);QueryPerformanceFrequency(&freq);lastPickMs=1000.0*(end.QuadPart-pickStart.QuadPart)/freq.QuadPart;
    if(blacklistPicking){blacklistRecognized(ok,path,target,selected?problem:L"已取消选取，可重新选择窗口。");return;}
    ShowWindow(editor,SW_SHOW);SetForegroundWindow(editor);
    if(selected&&!ok)error(editor,problem);
    if(ok){SetWindowTextW(fields[0],name);SetWindowTextW(fields[1],target);SetWindowTextW(fields[2],L"");SetWindowTextW(fields[3],path);
        swprintf(editorStatus,128,L"已识别 %ls · %.1f ms",name,lastPickMs);InvalidateRect(editor,NULL,FALSE);}
    SetFocus(fields[0]);
}
static LRESULT CALLBACK EditProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    LRESULT color=dialogColors(msg,wp);if(color)return color;
    if(msg==WM_DRAWITEM&&drawModernButton((DRAWITEMSTRUCT*)lp))return TRUE;
    if(msg==WM_NCHITTEST){POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(w,&p);if(p.y<S(GLASS_HEADER_HEIGHT)&&p.x<S(390))return HTCAPTION;return HTCLIENT;}
    if(msg==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);
        paintDialogHeader(dc,r,editorTitle,L"LITELAUNCH  /  APP PROFILE");
        paintDialogField(dc,24,92,572,36,GetFocus()==fields[0]);
        paintDialogField(dc,24,166,474,36,GetFocus()==fields[1]);
        paintDialogField(dc,24,236,572,36,GetFocus()==fields[2]);
        paintDialogField(dc,24,306,474,36,GetFocus()==fields[3]);
        dialogFadeLine(dc,S(24),r.right-S(24),S(340),dialogColor,dialogBorder,255);
        paintDialogStatusLine(dc,editorStatus,350,r.right);EndPaint(w,&ps);return 0;}
    if(msg==WM_LBUTTONDOWN){
        const int areas[4][4]={{24,92,572,36},{24,166,474,36},{24,236,572,36},{24,306,474,36}};
        POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        for(int i=0;i<4;i++)if(p.x>=S(areas[i][0])&&p.x<S(areas[i][0]+areas[i][2])&&p.y>=S(areas[i][1])&&p.y<S(areas[i][1]+areas[i][3])){
            if(IsWindow(fields[i])){POINT inside=p;MapWindowPoints(w,fields[i],&inside,1);if(inside.y<0)inside.y=0;
                SetFocus(fields[i]);LRESULT index=SendMessageW(fields[i],EM_CHARFROMPOS,0,MAKELPARAM(inside.x,inside.y));
                SendMessageW(fields[i],EM_SETSEL,LOWORD(index),LOWORD(index));}
            return 0;
        }
    }
    if(msg==WM_COMMAND) {
        if((HIWORD(wp)==EN_SETFOCUS||HIWORD(wp)==EN_KILLFOCUS)&&LOWORD(wp)>=10&&LOWORD(wp)<=13){InvalidateRect(w,NULL,FALSE);return 0;}
        if(LOWORD(wp)==103){beginPick();return 0;}
        if(LOWORD(wp)==101||LOWORD(wp)==102) {
            WCHAR path[2048]=L"";
            if(browse(w,path,LOWORD(wp)==102)) {
                if(LOWORD(wp)==102)SetWindowTextW(fields[3],path);
                else {Entry profile={0};if(!profileFromPath(path,&profile))error(w,L"无法读取快捷方式对应的应用。请检查快捷方式目标。");
                    else {SetWindowTextW(fields[0],profile.name);SetWindowTextW(fields[1],profile.target);
                        SetWindowTextW(fields[2],profile.args);SetWindowTextW(fields[3],profile.icon);}}
            }
        } else if(LOWORD(wp)==IDOK) {
            Entry old=entries[editIndex], next={0};
            GetWindowTextW(fields[0],next.name,128);GetWindowTextW(fields[1],next.target,2048);
            GetWindowTextW(fields[2],next.args,2048);GetWindowTextW(fields[3],next.icon,1024);
            if(!next.name[0]||!next.target[0]) { error(w,L"请填写名称和启动目标。");return 0; }
            normalize(next.target);
            entries[editIndex]=next;
            if(!saveConfig()) {entries[editIndex]=old;return 0;}
            if(old.image)DestroyIcon(old.image);if(old.visual)glassFreeIcon(old.visual);
            loadIcon(&entries[editIndex]);saved=TRUE;DestroyWindow(w);
        } else if(LOWORD(wp)==IDCANCEL)DestroyWindow(w);
        return 0;
    }
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
    return DefWindowProcW(w,msg,wp,lp);
}
static void editEntry(int index) {
    if(editing||index<0||index>=COUNT)return;
    editing=TRUE;saved=FALSE;editIndex=index;
    RECT pr;GetWindowRect(panel,&pr);
    MONITORINFO mi={0};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromWindow(panel,MONITOR_DEFAULTTONEAREST),&mi);
    int ew=S(620),eh=S(434);
    int ex=pr.left,ey=pr.top;
    if(ex+ew>mi.rcWork.right)ex=mi.rcWork.right-ew;if(ey+eh>mi.rcWork.bottom)ey=mi.rcWork.bottom-eh;
    if(ex<mi.rcWork.left)ex=mi.rcWork.left;if(ey<mi.rcWork.top)ey=mi.rcWork.top;
    lstrcpyW(editorTitle,entries[index].target[0]?L"编辑应用":L"添加应用");editorStatus[0]=0;
    editor=CreateWindowExW(WS_EX_TOPMOST,L"LiteLaunch.Editor",editorTitle,WS_POPUP|WS_CLIPCHILDREN,
        ex,ey,ew,eh,NULL,NULL,instance,NULL);
    if(!editor){editing=FALSE;error(panel,L"无法创建添加应用窗口。");return;}activeDialog=editor;
    roundDialogWindow(editor,18);
    Entry *e=&entries[index];
    control(L"BUTTON",L"选取窗口",WS_TABSTOP|BS_OWNERDRAW,460,16,110,32,103);
    control(L"BUTTON",L"×",WS_TABSTOP|BS_OWNERDRAW,584,16,24,32,IDCANCEL);
    control(L"STATIC",L"显示名称",0,24,70,300,18,0);
    fields[0]=control(L"EDIT",e->name,WS_TABSTOP|ES_AUTOHSCROLL,24,92,572,36,10);
    control(L"STATIC",L"启动目标  ·  程序、快捷方式或AppsFolder标识",0,24,144,460,18,0);
    fields[1]=control(L"EDIT",e->target,WS_TABSTOP|ES_AUTOHSCROLL,24,166,474,36,11);
    control(L"BUTTON",L"浏览…",WS_TABSTOP|BS_OWNERDRAW,508,166,88,36,101);
    control(L"STATIC",L"启动参数  ·  可留空",0,24,214,300,18,0);
    fields[2]=control(L"EDIT",e->args,WS_TABSTOP|ES_AUTOHSCROLL,24,236,572,36,12);
    control(L"STATIC",L"自定义图标  ·  留空时读取应用自带图标",0,24,284,400,18,0);
    fields[3]=control(L"EDIT",e->icon,WS_TABSTOP|ES_AUTOHSCROLL,24,306,474,36,13);
    control(L"BUTTON",L"浏览…",WS_TABSTOP|BS_OWNERDRAW,508,306,88,36,102);
    control(L"BUTTON",L"取消",WS_TABSTOP|BS_OWNERDRAW,412,382,86,36,IDCANCEL);
    control(L"BUTTON",L"保存",WS_TABSTOP|BS_OWNERDRAW,508,382,88,36,IDOK);
    SendMessageW(fields[0],EM_SETLIMITTEXT,127,0);SendMessageW(fields[1],EM_SETLIMITTEXT,2047,0);
    SendMessageW(fields[2],EM_SETLIMITTEXT,2047,0);SendMessageW(fields[3],EM_SETLIMITTEXT,1023,0);
    EnableWindow(panel,FALSE);ShowWindow(panel,SW_HIDE);ShowWindow(editor,SW_SHOW);SetForegroundWindow(editor);SetFocus(fields[0]);
    MSG msg;
    while(IsWindow(editor)) {
        int r=GetMessageW(&msg,NULL,0,0);
        if(r<=0){if(r==0)PostQuitMessage((int)msg.wParam);break;}
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE){DestroyWindow(editor);continue;}
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN){SendMessageW(editor,WM_COMMAND,IDOK,0);continue;}
        if(!IsDialogMessageW(editor,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    activeDialog=NULL;editor=NULL;EnableWindow(panel,TRUE);editing=FALSE;ShowWindow(panel,SW_SHOW);SetForegroundWindow(panel);SetFocus(panel);
    if(saved){updateRows();InvalidateRect(panel,NULL,FALSE);}
}
static BOOL startTarget(Entry *e,DWORD *failure) {
    WCHAR target[4096];ExpandEnvironmentStringsW(e->target,target,4096);
    SHELLEXECUTEINFOW se={0};se.cbSize=sizeof(se);
    WCHAR directory[4096];lstrcpyW(directory,target);WCHAR *lastSlash=wcsrchr(directory,L'\\');
    LPCWSTR extension=wcsrchr(target,L'.');
    if(lastSlash&&extension&&!_wcsicmp(extension,L".exe")){*lastSlash=0;se.lpDirectory=directory;}
    se.hwnd=panel;se.lpVerb=L"open";se.nShow=SW_SHOWNORMAL;se.fMask=SEE_MASK_FLAG_NO_UI;
    se.lpParameters=e->args[0]?e->args:NULL;
    BOOL ok=FALSE;DWORD code=0;
    if(!_wcsnicmp(target,L"shell:AppsFolder\\",17)) {
        IApplicationActivationManager *manager=NULL;
        HRESULT hr=CoCreateInstance(&CLSID_ApplicationActivationManager,NULL,CLSCTX_LOCAL_SERVER,&IID_IApplicationActivationManager,(void**)&manager);
        if(SUCCEEDED(hr)){DWORD pid=0;hr=IApplicationActivationManager_ActivateApplication(manager,target+17,e->args,AO_NOERRORUI,&pid);IApplicationActivationManager_Release(manager);}
        ok=SUCCEEDED(hr);code=(DWORD)hr;
    }else {se.lpFile=target;ok=ShellExecuteExW(&se);code=ok?ERROR_SUCCESS:GetLastError();}
    *failure=code;return ok;
}
static void launch(int i) {
    if(i<0||i>=COUNT)return;
    Entry *e=&entries[i];
    if(!e->target[0]){PostMessageW(panel,WM_OPENENTRY,(WPARAM)i,0);return;}
    WCHAR expanded[4096]=L"";ExpandEnvironmentStringsW(e->target,expanded,4096);
    HWND existing=hiddenTargetWindow(expanded);
    DWORD code=0;BOOL ok=startTarget(e,&code);
    runtimeLog(L"application launch slot=%d target=%ls ok=%d code=0x%08lX",i,e->target,ok,code);
    BOOL restored=revealTargetWindow(existing,i);
    if(ok||restored){
        if(!pinned)ShowWindow(panel,SW_HIDE);
    }
    else {WCHAR message[512];swprintf(message,512,L"启动失败（错误0x%08lX）。请检查目标、应用是否已安装以及启动参数。",code);error(panel,message);}
}
static void launchCell(int cell){
    int app=appIndexForCell(cell);if(app>=0){launch(app);return;}
    if(cell>=0&&cell<FUNCTION_COUNT){WCHAR id[64],label[128];pluginSlot(cell,id,label);ShowWindow(panel,SW_HIDE);invokePlugin(panel,id,cell,sourceWindow,L"");return;}
    if(cell==FUNCTION_COUNT){Entry action={0};UINT length=GetSystemDirectoryW(action.target,2048);if(!length||length+13>=2048){error(panel,L"无法确定控制面板路径。");return;}lstrcatW(action.target,L"\\control.exe");DWORD code=0;if(startTarget(&action,&code)){if(!pinned)ShowWindow(panel,SW_HIDE);}else{WCHAR message[160];swprintf(message,160,L"无法打开控制面板（0x%08lX）。",code);error(panel,message);}}
}
static BOOL reorderEntry(int from,int to){
    if(from<0||from>=COUNT||to<0||to>=COUNT||from==to)return FALSE;
    static Entry backup[COUNT];memcpy(backup,entries,sizeof(entries));Entry moved=entries[from];
    if(from<to)memmove(entries+from,entries+from+1,(to-from)*sizeof(Entry));
    else memmove(entries+to+1,entries+to,(from-to)*sizeof(Entry));
    entries[to]=moved;
    if(!saveConfig()){memcpy(entries,backup,sizeof(entries));return FALSE;}return TRUE;
}
static BOOL lastLayeredPaintOk;
static void paint(HWND w) {
    glassRows(displayRows*4,scrollRow);
    PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);
    HDC mem=CreateCompatibleDC(dc);void *pixels=NULL;
    BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=r.right;bi.bmiHeader.biHeight=-r.bottom;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,NULL,0);
    if(!bm){DeleteDC(mem);EndPaint(w,&ps);return;}HGDIOBJ old=SelectObject(mem,bm);
    const WCHAR *names[CELL_COUNT];void *icons[CELL_COUNT];BOOL occupied[CELL_COUNT];
    static WCHAR pluginLabels[4][128];
    for(int i=0;i<CELL_COUNT;i++){names[i]=L"";icons[i]=NULL;occupied[i]=FALSE;}
    for(int i=0;i<FUNCTION_COUNT;i++){WCHAR id[64];pluginSlot(i,id,pluginLabels[i]);names[i]=pluginLabels[i];occupied[i]=id[0]!=0;}
    static void *controlVisual=NULL;if(!controlVisual)controlVisual=glassLoadControlPanelIcon();
    names[FUNCTION_COUNT]=L"控制面板";icons[FUNCTION_COUNT]=controlVisual;occupied[FUNCTION_COUNT]=TRUE;
    for(int i=0;i<COUNT;i++){int cell=i+APP_CELL_START;names[cell]=entries[i].name;icons[cell]=entries[i].visual;occupied[cell]=entries[i].target[0]!=0;}
    POINT pointer;GetCursorPos(&pointer);ScreenToClient(w,&pointer);
    glassInteraction(pointer.x*96.f/dpi,pointer.y*96.f/dpi,iconDragging?pressedIndex:-1,hover);
    glassPaint(mem,r.right,r.bottom,dpi,hover,selection,pinned,names,icons,occupied);
    if(GetWindowLongW(w,GWL_EXSTYLE)&WS_EX_LAYERED){
        GdiFlush();glassMaskPixels(pixels,r.right,r.bottom,dpi);
        RECT wr;GetWindowRect(w,&wr);POINT dst={wr.left,wr.top},src={0,0};SIZE size={r.right,r.bottom};BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        lastLayeredPaintOk=UpdateLayeredWindow(w,dc,&dst,&size,mem,&src,0,&blend,ULW_ALPHA);
    }else BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bm);DeleteDC(mem);EndPaint(w,&ps);
}
static LRESULT CALLBACK HelpProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    LRESULT color=dialogColors(msg,wp);if(color)return color;
    if(msg==WM_DRAWITEM&&drawModernButton((DRAWITEMSTRUCT*)lp))return TRUE;
    if(msg==WM_NCHITTEST){POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(w,&p);if(p.y<S(GLASS_HEADER_HEIGHT)&&p.x<S(390))return HTCAPTION;return HTCLIENT;}
    if(msg==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);
        paintDialogHeader(dc,r,L"使用说明",L"LITELAUNCH  /  GUIDE");
        const WCHAR *lines[]={
            L"鼠标中键：显示或隐藏面板",
            L"第一排四个插件槽；下方第一格固定控制面板",
            L"名称每行编辑；后悔药跨窗口保存上次改名",
            L"点击应用区空位选取窗口，或拖入程序与快捷方式",
            L"右键应用：编辑或删除；选窗时右键或Esc取消",
            L"拖动应用卡片排序，功能栏保持固定",
            L"固定：启动或失去焦点后保留面板",
            L"Esc / ×：隐藏至托盘；托盘右键可退出"};
        /* The whole list lives on one soft card; accent dots carry the rhythm. */
        RECT card={S(20),S(70),r.right-S(20),S(318)};
        if(!smoothDialogRoundRect(dc,card,UI_R_CARD,inputColor,dialogBorder,TRUE))
            roundDialogRect(dc,card,UI_R_CARD,inputColor,dialogBorder,1);
        SetBkMode(dc,TRANSPARENT);SelectObject(dc,font);
        for(int i=0;i<8;i++){
            int top=S(78+i*29);COLORREF accent=glassAccentColor();
            RECT dot={S(34),top+S(9),S(40),top+S(15)};
            if(!smoothDialogCircle(dc,dot,accent,accent)){
                HBRUSH bullet=CreateSolidBrush(accent);FillRect(dc,&dot,bullet);DeleteObject(bullet);}
            SetTextColor(dc,dialogInk);SelectObject(dc,font);
            RECT line={S(52),top,r.right-S(32),S(105+i*29)};
            DrawTextW(dc,lines[i],-1,&line,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);}
        dialogFadeLine(dc,S(27),r.right-S(27),S(326),dialogColor,dialogBorder,255);
        SetTextColor(dc,dialogMuted);
        RECT configLine={S(27),S(336),r.right-S(27),S(362)};
        DrawTextW(dc,L"配置：%LOCALAPPDATA%\\LiteLaunch\\LiteLaunch.ini",-1,&configLine,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        RECT performanceLine={S(27),S(365),r.right-S(27),S(391)};
        DrawTextW(dc,L"列表异常少项时先退出，再核对同目录 LiteLaunch.initial.ini 手动恢复。",-1,&performanceLine,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        EndPaint(w,&ps);return 0;}
    if(msg==WM_COMMAND&&(LOWORD(wp)==IDCANCEL||LOWORD(wp)==IDOK)){DestroyWindow(w);return 0;}
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
    return DefWindowProcW(w,msg,wp,lp);
}
static void showHelp(void){
    if(editing&&activeDialog){SetForegroundWindow(activeDialog);return;}
    WNDCLASSW wc={0};wc.lpfnWndProc=HelpProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);
    wc.hbrBackground=dialogBackground;wc.lpszClassName=L"LiteLaunch.Help";RegisterClassW(&wc);
    BOOL wasVisible=IsWindowVisible(panel);POINT point;GetCursorPos(&point);
    MONITORINFO mi={0};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&mi);
    int width=S(620),height=S(464),x=point.x-width/2,y=point.y-S(35);
    if(x+width>mi.rcWork.right)x=mi.rcWork.right-width;if(y+height>mi.rcWork.bottom)y=mi.rcWork.bottom-height;
    if(x<mi.rcWork.left)x=mi.rcWork.left;if(y<mi.rcWork.top)y=mi.rcWork.top;
    HWND help=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"使用说明",WS_POPUP|WS_CLIPCHILDREN,x,y,width,height,NULL,NULL,instance,NULL);
    if(!help){error(panel,L"无法创建使用说明窗口。");return;}
    roundDialogWindow(help,18);editing=TRUE;activeDialog=help;if(wasVisible)ShowWindow(panel,SW_HIDE);
    HWND close=CreateWindowW(L"BUTTON",L"×",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,S(584),S(16),S(24),S(32),help,(HMENU)(INT_PTR)IDCANCEL,instance,NULL);
    HWND done=CreateWindowW(L"BUTTON",L"知道了",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,S(508),S(411),S(88),S(36),help,(HMENU)(INT_PTR)IDOK,instance,NULL);
    SendMessageW(close,WM_SETFONT,(WPARAM)font,TRUE);SendMessageW(done,WM_SETFONT,(WPARAM)font,TRUE);
    ShowWindow(help,SW_SHOW);SetForegroundWindow(help);SetFocus(done);
    MSG msg;while(IsWindow(help)){
        int got=GetMessageW(&msg,NULL,0,0);if(got<=0){if(!got)PostQuitMessage(0);break;}
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){DestroyWindow(help);continue;}
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN){SendMessageW(help,WM_COMMAND,IDOK,0);continue;}
        if(!IsDialogMessageW(help,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    activeDialog=NULL;editing=FALSE;
    if(wasVisible){ShowWindow(panel,SW_SHOW);SetForegroundWindow(panel);SetFocus(panel);}
}
typedef struct { MSAAMENUINFO accessibility;LPCWSTR label;int theme;BOOL separator,submenu,danger; } MenuVisual;
static int chosenScale;
static LRESULT CALLBACK ScaleProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    LRESULT color=dialogColors(msg,wp);if(color)return color;
    if(msg==WM_DRAWITEM&&drawModernButton((DRAWITEMSTRUCT*)lp))return TRUE;
    if(msg==WM_NCHITTEST){POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(w,&p);if(p.y<S(GLASS_HEADER_HEIGHT)&&p.x<S(220))return HTCAPTION;return HTCLIENT;}
    if(msg==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);
        paintDialogHeader(dc,r,L"自定义缩放",L"LITELAUNCH  /  SCALE");
        paintDialogField(dc,25,116,310,36,GetFocus()==GetDlgItem(w,201));
        paintDialogStatusLine(dc,L"面板尺寸、字体与图标按同一比例调整。",86,r.right);
        EndPaint(w,&ps);return 0;}
    if(msg==WM_COMMAND){int id=LOWORD(wp);if(id==IDOK){WCHAR value[24];GetWindowTextW(GetDlgItem(w,201),value,24);
            int percent=_wtoi(value);if(percent<60||percent>200){MessageBoxW(w,L"请输入60到200之间的整数百分比。",L"自定义缩放",MB_OK|MB_ICONINFORMATION);return 0;}
            chosenScale=percent;DestroyWindow(w);return 0;}
        if(id==IDCANCEL){DestroyWindow(w);return 0;}}
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
    return DefWindowProcW(w,msg,wp,lp);
}
static int askScale(void){
    WNDCLASSW cls={0};cls.lpfnWndProc=ScaleProc;cls.hInstance=instance;cls.hCursor=LoadCursorW(NULL,IDC_ARROW);cls.hbrBackground=dialogBackground;cls.lpszClassName=L"LiteLaunch.Scale";RegisterClassW(&cls);
    RECT r;GetWindowRect(panel,&r);chosenScale=0;
    HWND dialog=CreateWindowExW(WS_EX_TOPMOST,cls.lpszClassName,L"自定义缩放",WS_POPUP|WS_CLIPCHILDREN,
        r.left+S(28),r.top+S(30),S(360),S(224),panel,NULL,instance,NULL);
    if(!dialog)return 0;
    roundDialogWindow(dialog,18);
    HWND label=CreateWindowW(L"STATIC",L"面板与窗口缩放（60%—200%）",WS_CHILD|WS_VISIBLE,S(26),S(66),S(308),S(18),dialog,NULL,instance,NULL);
    WCHAR value[24];swprintf(value,24,L"%d",scalePercent);
    /* Same inset convention as control(): stay strictly inside the painted field. */
    int editInset=(S(3)>3)?S(3):3,editVertical=(S(8)>4)?S(8):4;
    HWND edit=CreateWindowExW(0,L"EDIT",value,WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,
        S(25)+editInset,S(116)+editVertical,S(310)-editInset*2,S(36)-editVertical*2,dialog,(HMENU)201,instance,NULL);
    roundFieldControl(edit);
    HWND apply=CreateWindowW(L"BUTTON",L"应用",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON|BS_OWNERDRAW,S(25),S(164),S(148),S(38),dialog,(HMENU)IDOK,instance,NULL);
    HWND cancel=CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,S(187),S(164),S(148),S(38),dialog,(HMENU)IDCANCEL,instance,NULL);
    HWND controls[]={label,edit,apply,cancel};
    for(int i=0;i<4;i++)SendMessageW(controls[i],WM_SETFONT,(WPARAM)font,TRUE);
    SendMessageW(edit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(S(12),S(12)));
    EnableWindow(panel,FALSE);ShowWindow(dialog,SW_SHOW);SetForegroundWindow(dialog);SetFocus(edit);
    MSG msg;while(IsWindow(dialog)){int got=GetMessageW(&msg,NULL,0,0);if(got<=0){if(got==0)PostQuitMessage((int)msg.wParam);break;}
        if(!IsDialogMessageW(dialog,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    EnableWindow(panel,TRUE);SetForegroundWindow(panel);return chosenScale;
}
static void menuVisualAppend(HMENU menu,UINT flags,UINT_PTR id,MenuVisual *visual){
    visual->accessibility.dwMSAASignature=MSAA_MENU_SIG;
    visual->accessibility.cchWText=visual->separator?0:(DWORD)wcslen(visual->label);
    visual->accessibility.pszWText=(LPWSTR)visual->label;
    AppendMenuW(menu,flags|MF_OWNERDRAW,id,(LPCWSTR)visual);
}
static BOOL menuVisualMeasure(MEASUREITEMSTRUCT *item){
    if(!item||item->CtlType!=ODT_MENU||!item->itemData)return FALSE;
    MenuVisual *visual=(MenuVisual*)item->itemData;
    item->itemWidth=S(visual->theme>=0?195:226);
    item->itemHeight=S(visual->separator?13:34);
    return TRUE;
}
static BOOL menuVisualDraw(DRAWITEMSTRUCT *item){
    if(!item||item->CtlType!=ODT_MENU||!item->itemData)return FALSE;
    MenuVisual *visual=(MenuVisual*)item->itemData;RECT r=item->rcItem;
    HBRUSH white=CreateSolidBrush(inputColor);FillRect(item->hDC,&r,white);DeleteObject(white);
    if(visual->separator){HPEN line=CreatePen(PS_SOLID,1,dialogBorder);HGDIOBJ old=SelectObject(item->hDC,line);
        int y=(r.top+r.bottom)/2;MoveToEx(item->hDC,r.left+S(12),y,NULL);LineTo(item->hDC,r.right-S(12),y);SelectObject(item->hDC,old);DeleteObject(line);return TRUE;}
    BOOL selected=(item->itemState&ODS_SELECTED)!=0,disabled=(item->itemState&ODS_DISABLED)!=0,checked=(item->itemState&ODS_CHECKED)!=0;
    if(selected&&!disabled){RECT highlight=r;InflateRect(&highlight,-S(4),-S(3));
        COLORREF accent=glassAccentColor();
        if(!smoothDialogRoundRect(item->hDC,highlight,UI_R_FIELD,uiBlend(inputColor,accent,9),uiBlend(dialogBorder,accent,34),TRUE))
            roundDialogRect(item->hDC,highlight,UI_R_FIELD,uiBlend(inputColor,accent,9),uiBlend(dialogBorder,accent,34),1);}
    int cy=(r.top+r.bottom)/2;
    if(visual->theme>=0){COLORREF swatch=uiThemeColor(visual->theme);
        RECT dotRect={r.left+S(13),cy-S(7),r.left+S(27),cy+S(7)};
        COLORREF dotBorder=uiBlend(swatch,dialogInk,18);
        /* The selected theme wears a ring *around* the swatch. Drawing it after
           the dot painted a near-white disc on top of it, which is why the
           current colour showed up as a white circle. */
        if(checked){RECT ring=dotRect;InflateRect(&ring,S(3),S(3));
            if(!smoothDialogCircle(item->hDC,ring,inputColor,uiBlend(swatch,dialogInk,36)))
                roundDialogRect(item->hDC,ring,UI_R_PILL,inputColor,uiBlend(swatch,dialogInk,36),1);}
        if(!smoothDialogCircle(item->hDC,dotRect,swatch,dotBorder)){
            HBRUSH dot=CreateSolidBrush(swatch);HPEN border=CreatePen(PS_SOLID,1,dotBorder);
            HGDIOBJ oldBrush=SelectObject(item->hDC,dot),oldPen=SelectObject(item->hDC,border);
            Ellipse(item->hDC,dotRect.left,dotRect.top,dotRect.right,dotRect.bottom);
            SelectObject(item->hDC,oldPen);SelectObject(item->hDC,oldBrush);DeleteObject(border);DeleteObject(dot);}
    }else if(checked){SetTextColor(item->hDC,glassAccentColor());SetBkMode(item->hDC,TRANSPARENT);SelectObject(item->hDC,font);
        RECT tick={r.left+S(10),r.top,r.left+S(32),r.bottom};DrawTextW(item->hDC,L"✓",-1,&tick,DT_SINGLELINE|DT_VCENTER|DT_CENTER|DT_NOPREFIX);}
    SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,disabled?dialogMuted:(visual->danger&&selected?RGB(180,57,70):dialogInk));SelectObject(item->hDC,font);
    RECT label={r.left+S(39),r.top,r.right-S(visual->submenu?27:15),r.bottom};
    DrawTextW(item->hDC,visual->label,-1,&label,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    if(visual->theme>=0&&checked){SetTextColor(item->hDC,glassAccentColor());RECT tick={r.right-S(31),r.top,r.right-S(12),r.bottom};
        DrawTextW(item->hDC,L"✓",-1,&tick,DT_SINGLELINE|DT_VCENTER|DT_CENTER|DT_NOPREFIX);}
    /* Windows reserves and paints the submenu chevron outside the owner-drawn item. */
    return TRUE;
}
static void menu(int index,POINT p) {
    HMENU m=CreatePopupMenu(),themes=CreatePopupMenu(),scales=CreatePopupMenu();
    HBRUSH menuBackground=CreateSolidBrush(inputColor);MENUINFO info={0};info.cbSize=sizeof(info);info.fMask=MIM_BACKGROUND;info.hbrBack=menuBackground;
    SetMenuInfo(m,&info);SetMenuInfo(themes,&info);SetMenuInfo(scales,&info);
    MenuVisual edit={.label=index>=0&&entries[index].target[0]?L"编辑应用…":L"添加应用…",.theme=-1};
    MenuVisual remove={.label=L"删除此应用",.theme=-1,.danger=TRUE},previous={.label=L"前移一格",.theme=-1},next={.label=L"后移一格",.theme=-1};
    MenuVisual separator={.label=L"",.theme=-1,.separator=TRUE},show={.label=L"显示面板",.theme=-1},pin={.label=L"固定面板",.theme=-1};
    MenuVisual themeMenu={.label=L"主题色",.theme=-1,.submenu=TRUE},scaleMenu={.label=L"界面缩放",.theme=-1,.submenu=TRUE},blocked={.label=L"中键唤醒黑名单…",.theme=-1};
    MenuVisual update={.label=L"检查GitHub更新…",.theme=-1},plugins={.label=L"插件管理…",.theme=-1};
    MenuVisual startup={.label=L"开机静默启动",.theme=-1},help={.label=L"使用说明",.theme=-1},quit={.label=L"退出轻启",.theme=-1};
    if(index>=0) {
        menuVisualAppend(m,0,1,&edit);
        if(entries[index].target[0]) {
            menuVisualAppend(m,0,2,&remove);
            if(index>0)menuVisualAppend(m,0,3,&previous);
            if(index<COUNT-1)menuVisualAppend(m,0,4,&next);
        }
        menuVisualAppend(m,MF_DISABLED,0,&separator);
    }
    menuVisualAppend(m,0,5,&show);menuVisualAppend(m,pinned?MF_CHECKED:0,6,&pin);
    const WCHAR *themeNames[UI_THEME_COUNT];for(int i=0;i<UI_THEME_COUNT;i++)themeNames[i]=uiThemeNames[i];
    MenuVisual themeItems[10];for(int i=0;i<10;i++){themeItems[i]=(MenuVisual){.label=themeNames[i],.theme=i};menuVisualAppend(themes,i==themeIndex?MF_CHECKED:0,20+i,&themeItems[i]);}
    menuVisualAppend(m,MF_POPUP,(UINT_PTR)themes,&themeMenu);
    const int percentages[]={75,90,100,110,125,150};const WCHAR *scaleNames[]={L"75%",L"90%",L"100%",L"110%",L"125%",L"150%"};
    MenuVisual scaleItems[7];for(int i=0;i<6;i++){scaleItems[i]=(MenuVisual){.label=scaleNames[i],.theme=-1};menuVisualAppend(scales,scalePercent==percentages[i]?MF_CHECKED:0,40+i,&scaleItems[i]);}
    scaleItems[6]=(MenuVisual){.label=L"自定义…",.theme=-1};menuVisualAppend(scales,0,46,&scaleItems[6]);
    menuVisualAppend(m,MF_POPUP,(UINT_PTR)scales,&scaleMenu);
    menuVisualAppend(m,0,9,&blocked);menuVisualAppend(m,startupEnabled()?MF_CHECKED:0,10,&startup);
    menuVisualAppend(m,0,12,&plugins);menuVisualAppend(m,0,11,&update);menuVisualAppend(m,0,7,&help);menuVisualAppend(m,MF_DISABLED,0,&separator);menuVisualAppend(m,0,8,&quit);
    SetForegroundWindow(panel);
    BOOL before=editing;editing=TRUE;
    int cmd=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,panel,NULL);
    editing=before;DestroyMenu(m);DeleteObject(menuBackground);PostMessageW(panel,WM_NULL,0,0);
    if(cmd==1)editEntry(index);
    if(cmd==2) {
        Entry old=entries[index];ZeroMemory(&entries[index],sizeof(Entry));
        if(saveConfig()){if(old.image)DestroyIcon(old.image);if(old.visual)glassFreeIcon(old.visual);}else entries[index]=old;
    }
    if(cmd==3||cmd==4) {
        int j=index+(cmd==3?-1:1);Entry tmp=entries[index];entries[index]=entries[j];entries[j]=tmp;
        if(!saveConfig()){tmp=entries[index];entries[index]=entries[j];entries[j]=tmp;}
    }
    if(cmd==5)showPanel();
    if(cmd==6)pinned=!pinned;
    if(cmd==7)showHelp();
    if(cmd==9)editBlacklist();
    if(cmd==11)showGitHubUpdate(panel);
    if(cmd==12)invokePlugin(panel,L"",-1,sourceWindow,L"");
    if(cmd==10){BOOL enabled=!startupEnabled();BOOL ok=setStartup(enabled);runtimeLog(L"startup menu enable=%d result=%d hr=0x%08lX",enabled,ok,(unsigned long)startupLastError());if(!ok){WCHAR message[256];swprintf(message,256,L"登录启动设置未完全更新（0x%08lX）。请检查任务计划程序与启动项。",(unsigned long)startupLastError());error(panel,message);}}
    if(cmd>=20&&cmd<30){int old=themeIndex;themeIndex=cmd-20;if(!saveConfig())themeIndex=old;glassTheme(themeIndex);}
    if(cmd>=40&&cmd<46)applyScale(percentages[cmd-40]);
    if(cmd==46){int percent=askScale();if(percent)applyScale(percent);}
    if(cmd==8)DestroyWindow(panel);else {updateRows();InvalidateRect(panel,NULL,FALSE);}
}
static LRESULT CALLBACK PanelProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==taskbarCreated && taskbarCreated){runtimeLog(L"Explorer taskbar recreated");ensureTray(TRUE);return 0;}
    switch(msg) {
        case WM_MEASUREITEM:if(menuVisualMeasure((MEASUREITEMSTRUCT*)lp))return TRUE;break;
        case WM_DRAWITEM:if(menuVisualDraw((DRAWITEMSTRUCT*)lp))return TRUE;break;
        case WM_PAINT:paint(w);return 0;
        case WM_SHOWWINDOW:if(wp)SetTimer(w,1,16,NULL);else KillTimer(w,1);return 0;
        case WM_WINDOWPOSCHANGED:{WINDOWPOS *pos=(WINDOWPOS*)lp;if(pos->flags&SWP_SHOWWINDOW)SetTimer(w,1,16,NULL);if(pos->flags&SWP_HIDEWINDOW)KillTimer(w,1);break;}
        case WM_TIMER:if(wp==TRAY_TIMER){ensureTray(FALSE);return 0;}if(wp==1&&IsWindowVisible(w)&&!IsIconic(w)){
            ULONGLONG now=GetTickCount64();RECT r;GetWindowRect(w,&r);if(liveCapture&&now-lastCaptureTick>=33){glassCapture(r.left,r.top,r.right-r.left,r.bottom-r.top);lastCaptureTick=now;}
            if(glassNeedsPaint())InvalidateRect(w,NULL,FALSE);
        }return 0;
        case WM_ERASEBKGND:return 1;
        case WM_HOTKEY:if(wp==2){finishPick(NULL);return 0;}if(editing){HWND current=activeDialog?activeDialog:pluginRenameActiveWindow();if(current){ShowWindow(current,IsIconic(current)?SW_RESTORE:SW_SHOW);SetForegroundWindow(current);}return 0;}if(!editing){if(IsWindowVisible(w))ShowWindow(w,SW_HIDE);else showPanel();}return 0;
        case WM_PICKHOVER:highlightPick((HWND)wp);return 0;
        case WM_OPENENTRY:editEntry((int)wp);return 0;
        case WM_PICK:finishPick((HWND)wp);return 0;
        case WM_CANCELPICK:finishPick(NULL);return 0;
        case WM_SHOWPANEL:if(editing){HWND current=activeDialog?activeDialog:pluginRenameActiveWindow();if(current){ShowWindow(current,IsIconic(current)?SW_RESTORE:SW_SHOW);SetForegroundWindow(current);}}else showPanel();return 0;
        case WM_COPYDATA:{COPYDATASTRUCT *request=(COPYDATASTRUCT*)lp;
            if(!request||request->dwData!=EVER_COPYDATA||!request->lpData||request->cbData<sizeof(WCHAR)||request->cbData>sizeof(everPendingCommand)||request->cbData%sizeof(WCHAR)||editing||everPendingReady)return FALSE;
            size_t n=request->cbData/sizeof(WCHAR);LPCWSTR text=(LPCWSTR)request->lpData;if(text[n-1]!=0)return FALSE;
            memcpy(everPendingCommand,text,request->cbData);everPendingSource=(HWND)wp;everPendingReady=TRUE;
            if(!PostMessageW(w,WM_EVERACTION,0,0)){everPendingReady=FALSE;return FALSE;}return TRUE;}
        case WM_EVERACTION:if(everPendingReady&&!editing){WCHAR action[8192];lstrcpynW(action,everPendingCommand,8192);HWND input=everPendingSource;everPendingReady=FALSE;ShowWindow(w,SW_HIDE);editing=TRUE;pluginRenameCommand(w,input,dpi,action);editing=FALSE;}return 0;
        case WM_EXITAPP:
            if(editing||everPendingReady||picking||dragging||iconDragging){runtimeLog(L"quit deferred: active editing or pending operation");return FALSE;}
            DestroyWindow(w);return TRUE;
        case WM_QUERYENDSESSION:
            if(lp&ENDSESSION_CLOSEAPP)return !editing&&!everPendingReady&&!picking&&!dragging&&!iconDragging;
            return TRUE;
        case WM_ENDSESSION:
            if(wp)DestroyWindow(w);
            return 0;
        case WM_MIDDLE_DIAGNOSTIC:runtimeLog(L"middle event accepted=%d flags=0x%04X blocked=%d (0=none 1=app 2=UU-viewer)",(BOOL)wp,(unsigned)LOWORD(lp),(int)HIWORD(lp));return 0;
        case WM_TRAY:
            if(lp==WM_LBUTTONUP)showPanel();
            if(lp==WM_RBUTTONUP){POINT p;GetCursorPos(&p);menu(-1,p);}return 0;
        case WM_MOUSEACTIVATE:rememberSourceWindow(GetForegroundWindow());return MA_ACTIVATE;
        case WM_ACTIVATE:rememberSourceWindow((HWND)lp);if(LOWORD(wp)==WA_INACTIVE&&!pinned&&!editing)ShowWindow(w,SW_HIDE);return 0;
        case WM_MOUSEMOVE:{if(dragging){POINT p;GetCursorPos(&p);SetWindowPos(w,NULL,windowStart.x+p.x-dragStart.x,windowStart.y+p.y-dragStart.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);return 0;}int mx=GET_X_LPARAM(lp),my=GET_Y_LPARAM(lp);BOOL keyboardCleared=selection>=0;selection=-1;
            int pressedApp=appIndexForCell(pressedIndex);if(pressedApp>=0&&!iconDragging&&entries[pressedApp].target[0]&&(abs(mx-pressPoint.x)>=GetSystemMetrics(SM_CXDRAG)||abs(my-pressPoint.y)>=GetSystemMetrics(SM_CYDRAG)))iconDragging=TRUE;
            int n=cellAt(mx,my);if(!selectableCell(n))n=-1;if(n!=hover||keyboardCleared||!dragging){hover=n;InvalidateRect(w,NULL,FALSE);}if(!tracking){TRACKMOUSEEVENT tm={sizeof(tm),TME_LEAVE,w,0};TrackMouseEvent(&tm);tracking=TRUE;}return 0;}
        case WM_MOUSEWHEEL:{int delta=GET_WHEEL_DELTA_WPARAM(wp);int next=scrollRow+(delta<0?1:-1);if(next>=0&&next<=displayRows-visibleRows){scrollRow=next;hover=selection=-1;glassResetSelection();InvalidateRect(w,NULL,FALSE);}return 0;}
        case WM_MOUSELEAVE:tracking=FALSE;hover=-1;InvalidateRect(w,NULL,FALSE);return 0;
        case WM_LBUTTONDOWN:{int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
            if(y<S(GLASS_HEADER_HEIGHT)){
                if(x>=S(panelWidth-30))ShowWindow(w,SW_HIDE);
                else if(x>=S(panelWidth-57)){POINT p;GetCursorPos(&p);menu(-1,p);}
                else if(x>=S(panelWidth-84)){pinned=!pinned;InvalidateRect(w,NULL,FALSE);}
                else {RECT r;GetWindowRect(w,&r);windowStart.x=r.left;windowStart.y=r.top;GetCursorPos(&dragStart);dragging=TRUE;SetCapture(w);}
            }else {selection=-1;pressedIndex=cellAt(x,y);if(!selectableCell(pressedIndex))pressedIndex=-1;pressPoint.x=x;pressPoint.y=y;iconDragging=FALSE;if(pressedIndex>=0)SetCapture(w);InvalidateRect(w,NULL,FALSE);}return 0;}
        case WM_LBUTTONUP:{
            if(dragging){dragging=FALSE;ReleaseCapture();return 0;}
            int from=pressedIndex,n=cellAt(GET_X_LPARAM(lp),GET_Y_LPARAM(lp));BOOL moved=iconDragging;
            pressedIndex=-1;iconDragging=FALSE;ReleaseCapture();selection=-1;
            if(moved){int fromApp=appIndexForCell(from),toApp=appIndexForCell(n);if(fromApp>=0&&toApp>=0&&reorderEntry(fromApp,toApp))glassReorder(from,n);updateRows();hover=selectableCell(n)?n:-1;InvalidateRect(w,NULL,FALSE);}
            else if(from>=0&&n==from)launchCell(n);
            return 0;}
        case WM_CAPTURECHANGED:if(lp&&((HWND)lp)!=w){dragging=FALSE;pressedIndex=-1;iconDragging=FALSE;}return 0;
        case WM_CANCELMODE:dragging=FALSE;pressedIndex=-1;iconDragging=FALSE;ReleaseCapture();return 0;
        case WM_CONTEXTMENU:{POINT p={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)},c=p;if(p.x==-1){GetCursorPos(&p);c=p;}ScreenToClient(w,&c);
            int pluginCell=cellAt(c.x,c.y);
            if(pluginCell>=0&&pluginCell<FUNCTION_COUNT){WCHAR id[64],label[128];pluginSlot(pluginCell,id,label);HMENU actions=pluginContextMenu(id);int action=TrackPopupMenu(actions,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,w,NULL);DestroyMenu(actions);if(action>=100&&action<104){ShowWindow(w,SW_HIDE);pluginRenameCommand(w,sourceWindow,dpi,pluginRenameMenuNames[action-100]);}if(action==PLUGIN_CONTEXT_UPDATE){ShowWindow(w,SW_HIDE);pluginCheckUpdate(w,id,pluginCell,sourceWindow);}if(action==110)invokePlugin(w,L"",pluginCell,sourceWindow,L"");return 0;}
            menu(appIndexForCell(cellAt(c.x,c.y)),p);return 0;}
        case WM_KEYDOWN:
            if(wp==VK_ESCAPE)ShowWindow(w,SW_HIDE);
            if(wp==VK_RETURN)launchCell(selection<0?0:selection);
            if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_UP||wp==VK_DOWN){hover=-1;int d=wp==VK_LEFT?-1:wp==VK_RIGHT?1:wp==VK_UP?-4:4,total=displayRows*4;selection=selection<0?0:(selection+d+total)%total;for(int skip=0;skip<total&&!selectableCell(selection);skip++)selection=(selection+d+total)%total;if(selection>=FUNCTION_COUNT){int appRow=(selection-FUNCTION_COUNT)/4,visibleApps=visibleRows-1;if(appRow<scrollRow)scrollRow=appRow;else if(appRow>=scrollRow+visibleApps)scrollRow=appRow-visibleApps+1;}InvalidateRect(w,NULL,FALSE);}return 0;
        case WM_DROPFILES:{
            HDROP drop=(HDROP)wp;UINT n=DragQueryFileW(drop,0xffffffff,NULL,0);static Entry backup[COUNT];memcpy(backup,entries,sizeof(entries));BOOL full=FALSE;
            BOOL invalid=FALSE;
            for(UINT k=0;k<n;k++) {int i=0;while(i<COUNT&&entries[i].target[0])i++;if(i==COUNT){full=TRUE;break;}
                WCHAR path[2048]=L"";DragQueryFileW(drop,k,path,2048);Entry profile={0};
                if(profileFromPath(path,&profile))entries[i]=profile;else invalid=TRUE;}
            DragFinish(drop);
            if(saveConfig()){for(int i=0;i<COUNT;i++)if(!backup[i].target[0]&&entries[i].target[0])loadIcon(&entries[i]);}else memcpy(entries,backup,sizeof(entries));
            if(full)error(w,L"已达到128个应用的容量。");
            if(invalid)error(w,L"部分快捷方式无法解析，未加入面板。请检查目标是否仍然存在。");updateRows();
            InvalidateRect(w,NULL,FALSE);return 0;}
        case WM_CLOSE:ShowWindow(w,SW_HIDE);return 0;
        case WM_DESTROY:KillTimer(w,TRAY_TIMER);Shell_NotifyIconW(NIM_DELETE,&tray);trayRegistered=FALSE;runtimeLog(L"normal window shutdown");PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE prev,LPWSTR cmd,int show) {
    (void)prev;(void)show;
    BOOL renameRequested=FALSE;WCHAR requestedAction[8192]=L"";HWND renameInputWindow=GetForegroundWindow();
    int argumentCount=0;LPWSTR *arguments=CommandLineToArgvW(GetCommandLineW(),&argumentCount);
    if(arguments){for(int i=1;i<argumentCount;i++){if(!wcscmp(arguments[i],L"--rename"))renameRequested=TRUE;else if(!wcscmp(arguments[i],L"--rename-action")){renameRequested=TRUE;if(i+1<argumentCount)lstrcpynW(requestedAction,arguments[++i],8192);}}LocalFree(arguments);}
    SetProcessDPIAware();instance=h;pinned=wcsstr(cmd,L"--pinned")!=NULL;
    WCHAR launchedExe[MAX_PATH]=L"";GetModuleFileNameW(NULL,launchedExe,MAX_PATH);runtimeLog(L"launch hidden=%d exe=%ls",wcsstr(cmd,L"--hidden")!=NULL,launchedExe);
    if(wcsstr(cmd,L"--enable-startup")||wcsstr(cmd,L"--disable-startup")){
        HRESULT initialized=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);BOOL enabled=wcsstr(cmd,L"--enable-startup")!=NULL;
        BOOL ok=setStartup(enabled);runtimeLog(L"startup control enable=%d result=%d hr=0x%08lX",enabled,ok,(unsigned long)startupLastError());
        if(!ok){WCHAR message[256];swprintf(message,256,L"登录启动设置未完全更新（0x%08lX）。请检查任务计划程序与启动项。",(unsigned long)startupLastError());error(NULL,message);}
        if(SUCCEEDED(initialized))CoUninitialize();return ok?0:4;
    }
    if(wcsstr(cmd,L"--quit")){HWND other=FindWindowW(CLASSNAME,NULL);BOOL sent=!other||PostMessageW(other,WM_EXITAPP,0,0);runtimeLog(L"quit control window=%p posted=%d",other,sent);return sent?0:5;}
    HANDLE mutex=CreateMutexW(NULL,FALSE,L"Local\\LiteLaunch.Native.Panel.v1");
    if(!mutex){runtimeLog(L"mutex creation failed error=%lu",GetLastError());return 3;}
    if(GetLastError()==ERROR_ALREADY_EXISTS){
        HWND other=FindWindowW(CLASSNAME,NULL);
        for(int retry=0;!other&&retry<100;retry++){Sleep(50);other=FindWindowW(CLASSNAME,NULL);}
        if(!other){runtimeLog(L"existing instance has no panel after 5 seconds; retryable startup failure");CloseHandle(mutex);return 3;}
        runtimeLog(L"existing instance found hwnd=%p",other);
        if(renameRequested){COPYDATASTRUCT request={EVER_COPYDATA,(DWORD)((wcslen(requestedAction)+1)*sizeof(WCHAR)),requestedAction};DWORD_PTR result=0;BOOL sent=SendMessageTimeoutW(other,WM_COPYDATA,(WPARAM)renameInputWindow,(LPARAM)&request,SMTO_ABORTIFHUNG,3000,&result)!=0;
            if(!sent||!result)MessageBoxW(NULL,L"现有LiteLaunch正在编辑，或尚未支持此重命名命令。请关闭编辑窗口，或完成新版安装后重试。",L"LiteLaunch",MB_ICONINFORMATION);CloseHandle(mutex);return sent&&result?0:6;}
        if(other&&!wcsstr(cmd,L"--hidden")){
            DWORD pid=0;GetWindowThreadProcessId(other,&pid);
            WCHAR running[MAX_PATH]=L"",requested[MAX_PATH]=L"";
            GetModuleFileNameW(NULL,requested,MAX_PATH);
            if(pid&&processPath(pid,running,MAX_PATH)&&_wcsicmp(running,requested)){
                WCHAR message[2*MAX_PATH+160];
                swprintf(message,2*MAX_PATH+160,L"已有 LiteLaunch 从以下位置运行：\n%ls\n\n本次启动的文件：\n%ls\n\n请先在托盘退出已有实例，再运行新文件。",running,requested);
                MessageBoxW(NULL,message,L"轻启 LiteLaunch · 已有实例",MB_OK|MB_ICONINFORMATION);
            } else PostMessageW(other,WM_SHOWPANEL,0,0);
        }
        if(mutex)CloseHandle(mutex);return 0;
    }
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    BOOL startupHealthy=repairStartup();runtimeLog(L"startup audit result=%d hr=0x%08lX",startupHealthy,(unsigned long)startupLastError());
    HDC screen=GetDC(NULL);systemDpi=GetDeviceCaps(screen,LOGPIXELSX);dpi=systemDpi;ReleaseDC(NULL,screen);
    if(!initConfigPaths()){
        runtimeLog(L"configuration directory initialization failed error=%lu",GetLastError());
        error(NULL,L"无法创建 %LOCALAPPDATA%\\LiteLaunch 配置目录或确定程序路径。LiteLaunch 已停止启动。\n请检查用户目录权限与路径长度。");
        CoUninitialize();if(mutex)CloseHandle(mutex);return 1;
    }
    WNDCLASSW wc={0};wc.lpfnWndProc=PanelProc;wc.hInstance=h;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hIcon=LoadIconW(h,MAKEINTRESOURCEW(1));if(!wc.hIcon)wc.hIcon=LoadIconW(NULL,IDI_APPLICATION);wc.lpszClassName=CLASSNAME;wc.style=0;
    RegisterClassW(&wc);wc.lpfnWndProc=EditProc;wc.lpszClassName=L"LiteLaunch.Editor";dialogBackground=CreateSolidBrush(dialogColor);inputBackground=CreateSolidBrush(inputColor);wc.hbrBackground=dialogBackground;wc.style=0;RegisterClassW(&wc);
    glassInit();
    if(!loadConfig()){
        runtimeLog(L"configuration load failed: %ls",configFailure);
        error(NULL,configFailure[0]?configFailure:L"LiteLaunch 配置加载失败，程序已停止启动。");
        glassShutdown();DeleteObject(font);DeleteObject(titleFont);DeleteObject(symbolFont);DeleteObject(metaFont);
        CoUninitialize();if(mutex)CloseHandle(mutex);return 1;
    }
    updateFonts();
    runtimeLog(L"configuration loaded targets=%d",configuredTargets(config));
    panel=CreateWindowExW((wcsstr(cmd,L"--inspect")?WS_EX_APPWINDOW:(WS_EX_TOOLWINDOW|WS_EX_LAYERED))|WS_EX_TOPMOST,CLASSNAME,L"轻启 LiteLaunch",WS_POPUP,0,0,S(panelWidth),S(GLASS_GRID_TOP+4*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT),NULL,NULL,h,NULL);
    if(!panel){runtimeLog(L"panel window creation failed error=%lu",GetLastError());return 2;}
    updateRows();
    if(renameRequested){lstrcpynW(everPendingCommand,requestedAction,8192);everPendingSource=renameInputWindow;everPendingReady=TRUE;PostMessageW(panel,WM_EVERACTION,0,0);}
    // Background capture before showing prevents feedback without hiding from UU.
    configurePanelCapture();
    DragAcceptFiles(panel,TRUE);
    tray.cbSize=sizeof(tray);tray.hWnd=panel;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=WM_TRAY;tray.hIcon=wc.hIcon;lstrcpyW(tray.szTip,L"轻启 · 鼠标中键");
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");BOOL trayOk=ensureTray(FALSE);
    hookReady=CreateEventW(NULL,TRUE,FALSE,NULL);
    hookThread=CreateThread(NULL,0,HookThreadProc,NULL,0,&hookThreadId);
    if(hookThread)WaitForSingleObject(hookReady,3000);
    runtimeLog(L"mouse hook thread=%lu hook=%p",hookThreadId,mouseHook);
    if(!wcsstr(cmd,L"--hidden"))showPanel();
    if(wcsstr(cmd,L"--inspect")) {
        WCHAR log[MAX_PATH];GetModuleFileNameW(NULL,log,MAX_PATH);WCHAR *end=wcsrchr(log,L'\\');if(end)lstrcpyW(end+1,L"inspect.txt");
        FILE *f=_wfopen(log,L"w");if(f){RECT r;GetWindowRect(panel,&r);fprintf(f,"visible=%d pinned=%d hwnd=%p rect=%ld,%ld,%ld,%ld tray=%d\n",IsWindowVisible(panel),pinned,panel,r.left,r.top,r.right,r.bottom,trayOk);fclose(f);}
    }
    MSG msg;int messageResult;while((messageResult=GetMessageW(&msg,NULL,0,0))>0){TranslateMessage(&msg);DispatchMessageW(&msg);}runtimeLog(L"message loop ended result=%d error=%lu",messageResult,messageResult<0?GetLastError():0);
    if(hookThread){PostThreadMessageW(hookThreadId,WM_QUIT,0,0);WaitForSingleObject(hookThread,3000);CloseHandle(hookThread);}if(hookReady)CloseHandle(hookReady);
    for(int i=0;i<COUNT;i++){if(entries[i].image)DestroyIcon(entries[i].image);if(entries[i].visual)glassFreeIcon(entries[i].visual);}
    glassShutdown();
    stopPickerVisual();if(dialogBackground)DeleteObject(dialogBackground);if(inputBackground)DeleteObject(inputBackground);DeleteObject(font);DeleteObject(titleFont);DeleteObject(symbolFont);DeleteObject(metaFont);CoUninitialize();if(mutex)CloseHandle(mutex);runtimeLog(L"process clean exit");return messageResult<0?6:0;
}
