/* Isolated regression and reproducible UI fixture; never uses Explorer selection. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
static int fixtureMessageBox(HWND,LPCWSTR,LPCWSTR,UINT) { return IDNO; }
#define MessageBoxW fixtureMessageBox
#define LITELAUNCH_RENAME_TESTING
#include "rename.cpp"
#include <cstdio>

using namespace llrename;
static int tests = 0;
static void check(bool ok, const char *name) {
    ++tests; if (!ok) { std::printf("FAIL %s\n",name); ExitProcess(2); }
    std::printf("PASS %s\n",name);
}
static void writeFile(const std::wstring &path,const char *text) {
    HANDLE h=CreateFileW(nativePath(path).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(h!=INVALID_HANDLE_VALUE,"fixture create"); DWORD wrote=0; WriteFile(h,text,(DWORD)strlen(text),&wrote,nullptr); CloseHandle(h);
}
static std::string readFile(const std::wstring &path) {
    HANDLE h=CreateFileW(nativePath(path).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return ""; char data[128]={}; DWORD n=0; ReadFile(h,data,127,&n,nullptr); CloseHandle(h); return std::string(data,n);
}
static Move move(const std::wstring&a,const std::wstring&b) { Move m; m.before=a; m.after=b; check(identify(a,m.identity),"fixture identity"); return m; }
static std::vector<Move> reverse(const std::vector<Move>&undo) { std::vector<Move> r; for(auto m:undo){std::swap(m.before,m.after);r.push_back(m);} return r; }
static void cleanupFile(const std::wstring&p) { DeleteFileW(nativePath(p).c_str()); }
static BOOL selectionIncludedParent=TRUE;
static HRESULT emptySelection(void *context,BOOL parentFallback,IShellItemArray **out) {
    selectionIncludedParent=parentFallback;*out=nullptr;
    if(!parentFallback)return S_FALSE;
    Com<IShellItem> folder;
    HRESULT hr=SHCreateItemFromParsingName(((const std::wstring*)context)->c_str(),nullptr,IID_PPV_ARGS(folder.out()));
    return FAILED(hr)?hr:SHCreateShellItemArrayFromShellItem(folder.p,IID_PPV_ARGS(out));
}
static bool snapshot(HWND hwnd,const std::wstring &path) {
    RECT rect;GetWindowRect(hwnd,&rect); int w=rect.right-rect.left,h=rect.bottom-rect.top;
    HDC dc=GetWindowDC(hwnd),memory=CreateCompatibleDC(dc); HBITMAP bitmap=CreateCompatibleBitmap(dc,w,h);
    HGDIOBJ old=SelectObject(memory,bitmap); BOOL printed=PrintWindow(hwnd,memory,2);
    BITMAPINFO info={}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    std::vector<BYTE> pixels((size_t)w*h*4); SelectObject(memory,old); GetDIBits(dc,bitmap,0,h,pixels.data(),&info,DIB_RGB_COLORS);
    BITMAPFILEHEADER file={};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);file.bfSize=file.bfOffBits+(DWORD)pixels.size();
    HANDLE out=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr); DWORD n=0;
    if(out!=INVALID_HANDLE_VALUE){WriteFile(out,&file,sizeof(file),&n,nullptr);WriteFile(out,&info.bmiHeader,sizeof(info.bmiHeader),&n,nullptr);WriteFile(out,pixels.data(),(DWORD)pixels.size(),&n,nullptr);CloseHandle(out);}
    DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(hwnd,dc);return printed && out!=INVALID_HANDLE_VALUE;
}
int wmain(int argc,wchar_t**argv) {
    wchar_t temp[MAX_PATH]={};GetTempPathW(MAX_PATH,temp);
    std::wstring base=std::wstring(temp)+L"LiteLaunch-Rename-QA-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    check(CreateDirectoryW(base.c_str(),nullptr)!=FALSE,"unique fixture directory");
    std::wstring a=base+L"\\A.txt",b=base+L"\\B.txt",c=base+L"\\外部.txt",casePath=base+L"\\Case.txt";
    writeFile(a,"original-A");writeFile(b,"original-B");writeFile(c,"external");writeFile(casePath,"case");
    std::vector<Entry> entries;check(addPath(entries,a),"add explicit file");check(!addPath(entries,a),"duplicate source ignored");
    Options o;o.prefix=L"中文_";o.find=L"A";o.replace=L"样本";std::wstring error;
    check(makePreview(entries,o,error) && entries[0].planned==L"中文_样本.txt","literal Chinese and extension preservation");
    o=Options();o.regex=true;o.find=L"(A)";o.replace=L"$1_匹配";
    check(makePreview(entries,o,error) && entries[0].planned==L"A_匹配.txt","regex capture and Chinese replacement");
    o.find=L"[";error.clear();check(!makePreview(entries,o,error),"invalid regex rejected");
    addPath(entries,b);o=Options();o.number=true;o.start=8;o.step=2;o.width=3;
    error.clear();check(makePreview(entries,o,error) && entries[0].planned==L"A_008.txt" && entries[1].planned==L"B_010.txt","number start step width");
    o=Options();o.letterCase=2;check(makePreview(entries,o,error) && entries[0].planned==L"a.txt","lowercase preserves extension");
    entries[0].hasManual=true;entries[0].manual=L"手工.md";check(makePreview(entries,o,error) && entries[0].planned==L"手工.md","manual full name override");
    for(auto name:{L"CON.txt",L"com1.log",L"NUL",L"bad?.txt",L"name.",L"tail ",L"..",L"A/B",L"LPT\u00b9.txt"}){
        std::wstring why;check(!validName(name,why),"invalid or reserved Windows name rejected");
    }
    check(transact({move(a,b),move(b,a)}).ok,"two phase swap executes");
    check(readFile(a)=="original-B" && readFile(b)=="original-A","swapped file contents preserved");
    Result swappedBack=transact({move(a,b),move(b,a)});check(swappedBack.ok && readFile(a)=="original-A" && readFile(b)=="original-B","swap undo restores originals");
    Result casing=transact({move(casePath,base+L"\\case.txt")});check(casing.ok,"case-only rename executes");
    WIN32_FIND_DATAW fd={};HANDLE find=FindFirstFileW((base+L"\\case.txt").c_str(),&fd);if(find!=INVALID_HANDLE_VALUE)FindClose(find);
    check(find!=INVALID_HANDLE_VALUE && std::wstring(fd.cFileName)==L"case.txt","case-only actual directory name updated");
    check(transact(reverse(casing.undo)).ok && readFile(casePath)=="case","case-only undo restores original");
    Result conflict=transact({move(a,c)});check(!conflict.ok && readFile(a)=="original-A" && readFile(c)=="external","external conflict blocks without changes");
    std::vector<Move> duplicates={move(a,base+L"\\dup.txt"),move(b,base+L"\\DUP.TXT")};
    check(!transact(duplicates).ok && readFile(a)=="original-A" && readFile(b)=="original-B","case-insensitive target duplicate blocks entire batch");
    HANDLE locked=CreateFileW(b.c_str(),GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(locked!=INVALID_HANDLE_VALUE,"rollback lock fixture");
    Result rollback=transact({move(a,base+L"\\NewA.txt"),move(b,base+L"\\NewB.txt")});CloseHandle(locked);
    check(!rollback.ok && rollback.undo.empty() && readFile(a)=="original-A" && readFile(b)=="original-B","mid-staging failure rolls back completed moves");
    Result renamed=transact({move(a,base+L"\\After.txt")});check(renamed.ok,"rename for undo fixture");
    writeFile(a,"new external occupant");
    check(!transact(reverse(renamed.undo)).ok && readFile(a)=="new external occupant" && readFile(base+L"\\After.txt")=="original-A","undo conflict cannot overwrite new occupant");
    cleanupFile(a);check(transact(reverse(renamed.undo)).ok && readFile(a)=="original-A","undo after resolving external conflict");
    std::vector<Move> history={move(a,base+L"\\HistoryA.txt"),move(b,base+L"\\HistoryB.txt")};
    Move incomplete=history[0];incomplete.before=history[0].after;incomplete.after=base+L"\\temporary-recovery.txt";
    reconcileUndo(history,{incomplete});
    check(history[0].before==a && history[0].after==incomplete.after,"partial undo retains original name and tracks actual current path");
    incomplete.before=incomplete.after;incomplete.after=a;reconcileUndo(history,{incomplete});
    check(history.size()==1 && history[0].before==b,"partial undo removes already recovered entry from retry history");
    Result beforeUndoFailure=transact({move(a,base+L"\\UndoA.txt"),move(b,base+L"\\UndoB.txt")});
    check(beforeUndoFailure.ok,"partial undo fixture executes initial batch");
    testMoveCall=0;testFailedMoves={4,5,6};Result partial=transact(reverse(beforeUndoFailure.undo));testFailedMoves.clear();
    check(!partial.ok && partial.undo.size()==1 && readFile(a)=="original-A" && readFile(base+L"\\UndoB.txt")=="original-B","injected finalize and rollback failures report real current paths");
    reconcileUndo(beforeUndoFailure.undo,partial.undo);
    check(transact(reverse(beforeUndoFailure.undo)).ok && readFile(a)=="original-A" && readFile(b)=="original-B","retry partial undo restores remaining original without overwriting recovered file");
    std::wstring folder=base+L"\\目录.含点";check(CreateDirectoryW(folder.c_str(),nullptr)!=FALSE,"folder fixture");
    std::vector<Entry> directories;addPath(directories,folder);o=Options();o.suffix=L"_尾";check(makePreview(directories,o,error) && directories[0].planned==L"目录.含点_尾","folder dots are not extensions");
    std::wstring child=folder+L"\\子.txt";writeFile(child,"nested");
    check(!transact({move(folder,base+L"\\目录新名"),move(child,folder+L"\\子新名.txt")}).ok,"nested parent and child rename rejected");
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    std::vector<Entry> none;check(!collectExplicitSelection(emptySelection,&folder,none) && !selectionIncludedParent && none.empty(),"empty explicit selection never adds containing folder");
    Com<IShellItemArray> implicit;check(SUCCEEDED(emptySelection(&folder,TRUE,implicit.out())) && implicit.p,"fixture confirms parent fallback would return a real folder");
    HWND owner=CreateWindowExW(0,L"STATIC",L"Owned QA window",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(owner!=nullptr,"modal owner fixture");renameTestThrowAfterCreate=true;renameShow(owner,nullptr,96);renameTestThrowAfterCreate=false;
    check(IsWindowEnabled(owner) && !FindWindowW(L"LiteLaunch.Native.Rename.v1",nullptr),"exception destroys modal window and restores enabled owner");
    EnableWindow(owner,FALSE);renameTestThrowAfterCreate=true;renameShow(owner,nullptr,96);renameTestThrowAfterCreate=false;
    check(!IsWindowEnabled(owner) && !FindWindowW(L"LiteLaunch.Native.Rename.v1",nullptr),"exception preserves previously disabled owner state");DestroyWindow(owner);
    if(implicit.p){implicit.p->Release();implicit.p=nullptr;}CoUninitialize();
    if(argc>2 && !wcscmp(argv[1],L"--ui")) {
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX cc={sizeof(cc),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&cc);
        WNDCLASSEXW wc={};wc.cbSize=sizeof(wc);wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"LiteLaunch.Rename.QA";RegisterClassExW(&wc);
        Dialog d;d.dpi=96;addPath(d.items,a);addPath(d.items,b);addPath(d.items,folder);
        HWND ui=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,L"批量重命名 · 隔离测试",WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,80,80,940,670,nullptr,nullptr,wc.hInstance,&d);
        check(ui!=nullptr,"native UI fixture opens");d.updating=true;SetWindowTextW(d.ctl(PREFIX),L"资料_");SendMessageW(d.ctl(NUMBER),BM_SETCHECK,BST_CHECKED,0);d.updating=false;d.preview();
        ShowWindow(ui,SW_SHOWNORMAL);UpdateWindow(ui);DWORD start=GetTickCount();MSG msg;
        while(GetTickCount()-start<900){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(20);}
        check(snapshot(ui,argv[2]),"native UI fixture screenshot");
        DestroyWindow(ui);CoUninitialize();
    }
    cleanupFile(child);RemoveDirectoryW(folder.c_str());cleanupFile(a);cleanupFile(b);cleanupFile(c);cleanupFile(casePath);
    check(RemoveDirectoryW(base.c_str())!=FALSE,"only owned fixture cleanup");
    std::printf("ALL PASS %d checks\n",tests);return 0;
}
