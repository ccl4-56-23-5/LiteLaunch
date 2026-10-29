#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
static HWND fixtureHit;
static int nextCalls;
static DWORD deniedPid;
static HANDLE WINAPI fixtureOpenProcess(DWORD a,BOOL b,DWORD pid){if(pid==deniedPid){SetLastError(ERROR_ACCESS_DENIED);return NULL;}return OpenProcess(a,b,pid);}
static HWND WINAPI fixturePoint(POINT p){(void)p;return fixtureHit;}
static LRESULT WINAPI fixtureNext(HHOOK h,int c,WPARAM w,LPARAM l){(void)h;(void)c;(void)w;(void)l;nextCalls++;return 77;}
#define OpenProcess fixtureOpenProcess
#define WindowFromPoint fixturePoint
#define CallNextHookEx fixtureNext
#define wWinMain LiteLaunchUiEntry
#include "main.c"
#undef wWinMain
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);result=1;goto cleanup;}}while(0)
static int messages(UINT type){MSG m;int n=0;while(PeekMessageW(&m,panel,type,type,PM_REMOVE))n++;return n;}
static LRESULT click(UINT type,DWORD flags){MSLLHOOKSTRUCT m={0};m.flags=flags;return MouseProc(HC_ACTION,type,(LPARAM)&m);}
int wmain(int argc,WCHAR **argv){
    instance=GetModuleHandleW(NULL);WNDCLASSW wc={0};wc.hInstance=instance;wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"LiteLaunch.MouseRouteFixture";RegisterClassW(&wc);
    if(argc==2&&!wcscmp(argv[1],L"--host")){
        WCHAR title[80];swprintf(title,80,L"Route host %lu",GetCurrentProcessId());
        HWND host=CreateWindowW(wc.lpszClassName,title,WS_OVERLAPPEDWINDOW,0,0,240,160,NULL,NULL,instance,NULL);
        if(!host)return 2;
        MSG m;while(IsWindow(host)&&GetMessageW(&m,NULL,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return 0;
    }
    if(argc==3&&!wcscmp(argv[1],L"--uu-system-probe")){
        blacklistCount=1;lstrcpyW(blacklist[0],L"GameViewer.exe");DWORD pid=(DWORD)wcstoul(argv[2],NULL,10);
        int physical=inaccessibleUuWindow(pid,0),injected=inaccessibleUuWindow(pid,LLMHF_INJECTED);
        printf("UU SYSTEM pid=%lu physical_block=%d injected_block=%d\n",pid,physical,injected);
        return physical==0&&injected==0?0:1;
    }
    int result=0;PROCESS_INFORMATION children[3]={0};HWND hosts[3]={0};
    CHECK(argc==2);panel=CreateWindowW(wc.lpszClassName,L"Private route sink",WS_POPUP,0,0,1,1,NULL,NULL,instance,NULL);CHECK(panel);
    WCHAR self[2048],paths[3][2048];GetModuleFileNameW(NULL,self,2048);
    const WCHAR *names[]={L"GameViewer.exe",L"GameViewerServer.exe",L"BLENDER.exe"};
    for(int i=0;i<3;i++){
        swprintf(paths[i],2048,L"%ls\\%ls",argv[1],names[i]);CHECK(CopyFileW(self,paths[i],FALSE));
        WCHAR command[2200];swprintf(command,2200,L"\"%ls\" --host",paths[i]);STARTUPINFOW si={0};si.cb=sizeof(si);
        CHECK(CreateProcessW(paths[i],command,NULL,NULL,FALSE,0,NULL,argv[1],&si,&children[i]));
        WCHAR title[80];swprintf(title,80,L"Route host %lu",children[i].dwProcessId);
        for(int n=0;n<200&&!hosts[i];n++){hosts[i]=FindWindowW(wc.lpszClassName,title);Sleep(10);}CHECK(hosts[i]);
    }
    blacklistCount=1;lstrcpyW(blacklist[0],paths[0]);
    DWORD flags[]={0,LLMHF_INJECTED,LLMHF_INJECTED|LLMHF_LOWER_IL_INJECTED};
    for(int i=0;i<3;i++){
        fixtureHit=hosts[0];CHECK(blockedAtPoint((POINT){0,0},flags[i])==2);
        CHECK(click(WM_MBUTTONDOWN,flags[i])==77);CHECK(messages(WM_HOTKEY)==0);
        fixtureHit=panel;CHECK(click(WM_MBUTTONUP,flags[i])==77);
    }
    puts("PASS UU viewer forwards physical and injected clicks without waking local panel");
    for(int i=0;i<3;i++){
        fixtureHit=hosts[1];CHECK(blockedAtPoint((POINT){0,0},flags[i])==0);
        CHECK(click(WM_MBUTTONDOWN,flags[i])==1);CHECK(messages(WM_HOTKEY)==1);
        CHECK(click(WM_MBUTTONDOWN,flags[i])==1);CHECK(messages(WM_HOTKEY)==0);
        fixtureHit=hosts[0];CHECK(click(WM_MBUTTONUP,flags[i])==1);
    }
    puts("PASS UU host server permits both driver and injected input; duplicate down wakes once");
    CHECK(inaccessibleUuWindow(children[0].dwProcessId,0)==2);
    CHECK(inaccessibleUuWindow(children[0].dwProcessId,LLMHF_INJECTED)==2);
    CHECK(inaccessibleUuWindow(children[1].dwProcessId,0)==0);
    CHECK(inaccessibleUuWindow(children[1].dwProcessId,LLMHF_INJECTED)==0);
    CHECK(inaccessibleUuWindow(children[2].dwProcessId,0)==0);
    deniedPid=children[1].dwProcessId;fixtureHit=hosts[1];
    CHECK(click(WM_MBUTTONDOWN,0)==1);CHECK(messages(WM_HOTKEY)==1);CHECK(click(WM_MBUTTONUP,0)==1);
    deniedPid=children[0].dwProcessId;fixtureHit=hosts[0];
    CHECK(click(WM_MBUTTONDOWN,LLMHF_INJECTED)==77);CHECK(messages(WM_HOTKEY)==0);CHECK(click(WM_MBUTTONUP,LLMHF_INJECTED)==77);deniedPid=0;
    puts("PASS AccessDenied fallback distinguishes protected viewer from host server without injection assumptions");
    fixtureHit=panel;CHECK(click(WM_MBUTTONDOWN,0)==1);CHECK(messages(WM_HOTKEY)==1);
    fixtureHit=hosts[0];CHECK(click(WM_MBUTTONDOWN,LLMHF_INJECTED)==77);CHECK(messages(WM_HOTKEY)==0);
    CHECK(click(WM_MBUTTONUP,0)==1);CHECK(click(WM_MBUTTONUP,LLMHF_INJECTED)==77);
    CHECK(click(WM_MBUTTONUP,0)==77);CHECK(MouseProc(-1,WM_MBUTTONDOWN,0)==77);
    puts("PASS independent input pairs latch down routing across cursor changes; orphan and negative-code events pass");
    lstrcpyW(blacklist[0],L"C:\\UU\\GameViewer.exe");
    CHECK(mouseBlacklistKind(L"c:\\uu\\GameViewer.exe",L"",LLMHF_INJECTED)==2);
    CHECK(mouseBlacklistKind(L"C:\\Different\\GameViewer.exe",L"",0)==0);
    CHECK(mouseBlacklistKind(L"C:\\UU\\GameViewerFake.exe",L"",0)==0);
    CHECK(mouseBlacklistKind(L"C:\\UU\\GameViewerServer.exe",L"",0)==0);
    blacklistCount=2;lstrcpyW(blacklist[1],L"Vendor.App!App");
    CHECK(mouseBlacklistKind(L"C:\\UU\\GameViewerServer.exe",L"Vendor.App!App",LLMHF_INJECTED)==1);
    puts("PASS full installation boundary, exact viewer names, and ordinary AppID blacklist priority");
    fixtureHit=hosts[2];blacklistCount=1;lstrcpyW(blacklist[0],paths[2]);
    CHECK(click(WM_MBUTTONDOWN,LLMHF_INJECTED)==77);CHECK(messages(WM_HOTKEY)==0);CHECK(click(WM_MBUTTONUP,LLMHF_INJECTED)==77);
    blacklistCount=2;lstrcpyW(blacklist[1],paths[0]);
    LONG_PTR oldStyle=GetWindowLongPtrW(hosts[2],GWL_STYLE);SetWindowLongPtrW(hosts[2],GWL_STYLE,(oldStyle&~WS_POPUP)|WS_CHILD);
    SetParent(hosts[2],hosts[1]);CHECK(GetParent(hosts[2])==hosts[1]);fixtureHit=hosts[1];
    CHECK(blockedAtPoint((POINT){0,0},LLMHF_INJECTED)==1);
    CHECK(click(WM_MBUTTONDOWN,LLMHF_INJECTED)==77);CHECK(messages(WM_HOTKEY)==0);CHECK(click(WM_MBUTTONUP,LLMHF_INJECTED)==77);
    SetParent(hosts[2],NULL);SetWindowLongPtrW(hosts[2],GWL_STYLE,oldStyle);
    LONG_PTR viewerStyle=GetWindowLongPtrW(hosts[0],GWL_STYLE);SetWindowLongPtrW(hosts[0],GWL_STYLE,(viewerStyle&~WS_POPUP)|WS_CHILD);
    SetParent(hosts[0],hosts[1]);CHECK(GetParent(hosts[0])==hosts[1]);fixtureHit=hosts[1];
    blacklistCount=1;lstrcpyW(blacklist[0],paths[0]);CHECK(blockedAtPoint((POINT){0,0},LLMHF_INJECTED)==2);
    CHECK(click(WM_MBUTTONDOWN,LLMHF_INJECTED)==77);CHECK(messages(WM_HOTKEY)==0);CHECK(click(WM_MBUTTONUP,LLMHF_INJECTED)==77);
    SetParent(hosts[0],NULL);SetWindowLongPtrW(hosts[0],GWL_STYLE,viewerStyle);
    puts("PASS host overlays do not block ordinary apps; hosted blacklist and nested remote viewer still forward");
    fixtureHit=hosts[0];picking=TRUE;
    CHECK(click(WM_MBUTTONDOWN,0)==1);CHECK(messages(WM_CANCELPICK)==1);CHECK(messages(WM_HOTKEY)==0);CHECK(click(WM_MBUTTONUP,0)==1);picking=FALSE;
    puts("PASS middle click cancels picking without opening panel");
cleanup:
    if(panel)DestroyWindow(panel);
    for(int i=0;i<3;i++)if(children[i].hProcess){if(hosts[i])PostMessageW(hosts[i],WM_CLOSE,0,0);if(WaitForSingleObject(children[i].hProcess,5000)!=WAIT_OBJECT_0)TerminateProcess(children[i].hProcess,3);CloseHandle(children[i].hThread);CloseHandle(children[i].hProcess);}
    return result;
}