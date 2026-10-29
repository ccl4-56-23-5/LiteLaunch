#define UNICODE
#define _UNICODE
#define LITELAUNCH_EVER_TESTING
#define LITELAUNCH_BATCH_PLUGIN
#include <windows.h>
#include <cstdio>
static int warnings=0;
static int WINAPI testMessage(HWND,LPCWSTR,LPCWSTR,UINT){warnings++;return IDNO;}
#define MessageBoxW testMessage
#include "../rename.cpp"
#undef MessageBoxW
using namespace llrename;
static int checks=0;
static void capturePreview(EverWindow&editor,const std::wstring&file){
    SetWindowTextW(editor.window,L"批量重命名 · 规则队列与预览");editor.batchList();editor.batchFiles();
    ShowWindow(editor.window,SW_SHOWNOACTIVATE);UpdateWindow(editor.window);RECT r;GetClientRect(editor.window,&r);
    HDC screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,r.right,r.bottom);HGDIOBJ old=SelectObject(memory,bitmap);
    PrintWindow(editor.window,memory,PW_CLIENTONLY);editor.paint(memory,r);
    for(HWND child=GetWindow(editor.window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        if(!IsWindowVisible(child))continue;RECT cr;GetWindowRect(child,&cr);POINT point={cr.left,cr.top};ScreenToClient(editor.window,&point);int saved=SaveDC(memory);SetViewportOrgEx(memory,point.x,point.y,nullptr);IntersectClipRect(memory,0,0,cr.right-cr.left,cr.bottom-cr.top);SendMessageW(child,WM_PRINT,(WPARAM)memory,PRF_CLIENT|PRF_ERASEBKGND|PRF_CHILDREN);RestoreDC(memory,saved);
    }
    SelectObject(memory,old);eGp::Bitmap picture(bitmap,nullptr);CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};picture.Save(file.c_str(),&png,nullptr);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(nullptr,screen);ShowWindow(editor.window,SW_HIDE);
}
static void check(bool value,const char*name){printf("%s %s\n",value?"PASS":"FAIL",name);if(!value)throw std::runtime_error(name);checks++;}
static batch::Step insert(const wchar_t*value){batch::Step s;s.rule.text=value;return s;}
static void engineTests(const std::wstring&root){
    std::vector<batch::Item> items={{root+L"\\PHOTO.JPG",L"PHOTO.JPG",false},{root+L"\\B.JPG",L"B.JPG",false}};std::vector<std::wstring> out;std::wstring error;
    batch::Step prefix=insert(L"old_"),replace;replace.rule.mode=ever::Mode::FindReplace;replace.rule.regex=false;replace.rule.replacements=L"old|new";batch::Step lower;lower.rule.operation=ever::Operation::Convert;lower.rule.conversion=ever::Conversion::Lower;
    check(batch::preview({prefix,replace,lower},items,out,error)&&out[0]==L"new_photo.JPG","rules consume previous result in top-to-bottom order and preserve extension");
    check(batch::preview({replace,prefix,lower},items,out,error)&&out[0]==L"old_photo.JPG","reordering changes the result");
    replace.enabled=false;check(batch::preview({prefix,replace},items,out,error)&&out[0]==L"old_PHOTO.JPG","unchecked rule is skipped");
    lower.skipExtension=false;check(batch::preview({lower},items,out,error)&&out[0]==L"photo.jpg","rule can include extension explicitly");
    batch::Step numbering;numbering.rule.mode=ever::Mode::NamingTemplate;numbering.rule.namingPattern=L"{name}-{num}";numbering.rule.sequenceStart=L"1";numbering.rule.sequenceWidth=3;numbering.rule.sequenceStep=2;
    check(batch::preview({numbering},items,out,error)&&out[0]==L"PHOTO-001.JPG"&&out[1]==L"B-003.JPG","numbering uses ordered batch start step and width");
    batch::Step extension;extension.kind=batch::Kind::Extension;extension.text=L"png";
    check(batch::preview({numbering,extension},items,out,error)&&out[0]==L"PHOTO-001.png","extension rule composes after numbering");
    auto folders=items;folders[0].folder=true;check(batch::preview({extension},folders,out,error)&&out[0]==L"PHOTO.JPG","extension rule does not change folder names");
    auto unicode=items;unicode[0].name=L"草稿🙂[A].jpg";batch::Step strip;strip.kind=batch::Kind::Strip;strip.text=L"🙂[]";check(batch::preview({strip},unicode,out,error)&&out[0]==L"草稿A.jpg","strip keeps Unicode surrogate pairs intact");
    batch::Step regex;regex.rule.mode=ever::Mode::FindReplace;regex.rule.regex=true;regex.rule.replacements=L"|=\u241e\nPHOTO|B\u241eShot";
    check(batch::preview({regex},items,out,error)&&out[0]==L"Shot.JPG"&&out[1]==L"Shot.JPG","regex alternation survives rule field encoding");
    std::vector<std::wstring> sentinel={L"unchanged"};numbering.rule.namingPattern=L"{taken}-{num}";check(!batch::preview({numbering},items,sentinel,error)&&sentinel[0]==L"unchanged","missing metadata fails atomically without overwriting prior preview");
    prefix.rule.text=L"bad\n";check(!batch::preview({prefix},items,sentinel,error)&&sentinel[0]==L"unchanged","newline output cannot shift filename rows");
    std::vector<batch::Step> tooMany(129);check(!batch::preview(tooMany,items,sentinel,error),"rule count is bounded");
}
static void windowTests(const std::wstring&root){
    std::wstring first=root+L"\\A.txt",second=root+L"\\B.txt";{std::ofstream a(first.c_str());a<<"first";std::ofstream b(second.c_str());b<<"second";}
    EverWindow editor;addPath(editor.items,first);addPath(editor.items,second);Scintilla_RegisterClasses(GetModuleHandleW(nullptr));
    WNDCLASSEXW cls={sizeof(cls)};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=EverWindow::proc;cls.lpszClassName=L"LiteLaunch.Batch.PrivateQA";RegisterClassExW(&cls);
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"Batch QA",WS_OVERLAPPEDWINDOW,-20000,-20000,1180,740,nullptr,nullptr,cls.hInstance,&editor);check(window&&editor.editor,"new rule and preview UI creates using production controls");
    check(GetDlgItem(window,EverWindow::BR_RULES)&&GetDlgItem(window,EverWindow::BR_FILES),"ordered rules and original/new preview table present");
    bool layoutValid=true;int visibleIds[]={EverWindow::BR_RULES,EverWindow::BR_FILES,EverWindow::BR_ADD,EverWindow::BR_EDIT,EverWindow::BR_UP,EverWindow::BR_DOWN,EverWindow::BR_DELETE,EverWindow::BR_CLEAR,EverWindow::BR_PREVIEW,EverWindow::BR_LEGACY,EverWindow::BR_SAVE,EverWindow::BR_LOAD,EE_EDITOR,EE_FIND,EE_OK,EE_ADD,EE_FOLDER,EE_READ,EE_OTHER,EE_STATUS};
    for(int scale:{72,96,120,144,192})for(int width:{920,1180})for(int height:{620,740}){editor.dpi=scale;SetWindowPos(window,nullptr,-20000,-20000,MulDiv(width,scale,96),MulDiv(height,scale,96),SWP_NOACTIVATE|SWP_NOZORDER);RECT client;GetClientRect(window,&client);for(int id:visibleIds){RECT box;GetWindowRect(editor.ctl(id),&box);MapWindowPoints(nullptr,window,(POINT*)&box,2);if(box.left<0||box.top<0||box.right>client.right||box.bottom>client.bottom||box.right<=box.left||box.bottom<=box.top)layoutValid=false;}}
    check(layoutValid,"all visible controls stay within compact and wide layouts at 75 to 200 percent scale");
    editor.dpi=96;SetWindowPos(window,nullptr,-20000,-20000,1180,740,SWP_NOACTIVATE|SWP_NOZORDER);
    check(ListView_GetItemCount(editor.ctl(EverWindow::BR_FILES))==2,"preview table includes selected files");
    editor.ruleStack={insert(L"X_")};editor.batchList(0);check(editor.batchPreview(false)&&editor.text()==L"X_A.txt\nX_B.txt","production pipeline updates editable names");
    check(attrs(first)!=INVALID_FILE_ATTRIBUTES&&attrs(root+L"\\X_A.txt")==INVALID_FILE_ATTRIBUTES,"preview never renames real files");
    auto encoded=editor.batchEncode();auto decoded=EverWindow::batchDecode(encoded);check(decoded.size()==1&&decoded[0].rule.text==L"X_","rule set round trip retains parameters");
    encoded[8]^=1;bool rejected=false;try{EverWindow::batchDecode(encoded);}catch(...){rejected=true;}check(rejected,"modified rule set rejected before applying");
    ListView_SetCheckState(editor.ctl(EverWindow::BR_RULES),0,FALSE);check(editor.text()==L"A.txt\nB.txt","native checkbox immediately disables one rule");
    ListView_SetCheckState(editor.ctl(EverWindow::BR_RULES),0,TRUE);editor.setText(L"Manual_A.txt\nManual_B.txt");int before=warnings;check(!editor.batchPreview()&&warnings==before+1&&editor.text().find(L"Manual_A")==0,"manual draft is protected from automatic recompute");
    check(editor.batchPreview(false),"explicit preview can recompute from originals");
    editor.toggle(true);batch::Step ext;ext.kind=batch::Kind::Extension;ext.text=L"zip";editor.ruleStack.push_back(ext);check(editor.batchPreview(false)&&editor.extensions[0]==L".zip"&&editor.text()==L"X_A\nX_B","hidden extension tracks extension changes atomically");
    check(editor.execute(),"execution commits composed names only on explicit command");
    check(attrs(root+L"\\X_A.zip")!=INVALID_FILE_ATTRIBUTES&&attrs(first)==INVALID_FILE_ATTRIBUTES,"composed target actually committed within private fixture");
    editor.regret();check(attrs(first)!=INVALID_FILE_ATTRIBUTES&&attrs(second)!=INVALID_FILE_ATTRIBUTES,"existing undo history restores new plugin renames");
    editor.items.clear();for(const wchar_t*name:{L"IMG_1024.jpg",L"IMG_1025.jpg",L"IMG_1026.jpg",L"IMG_1027.jpg"}){Entry entry;entry.path=root+L"\\"+name;entry.name=name;editor.items.push_back(entry);}
    batch::Step replacement;replacement.rule.mode=ever::Mode::FindReplace;replacement.rule.regex=false;replacement.rule.replacements=L"IMG_|旅行_";
    batch::Step sequence;sequence.rule.mode=ever::Mode::NamingTemplate;sequence.rule.namingPattern=L"{name}-{num}";sequence.rule.sequenceStart=L"1";sequence.rule.sequenceWidth=3;
    editor.hidden=false;editor.ruleStack={replacement,sequence};editor.rebuild();capturePreview(editor,root+L"\\preview.png");
    DestroyWindow(window);
}
int wmain(int argc,wchar_t**argv){
    setvbuf(stdout,nullptr,_IONBF,0);
    try{if(argc!=3)throw std::runtime_error("Expected new QA root and regex worker");std::wstring root=fullPath(argv[1]);if(fileName(root).find(L"LL_BATCH_QA_")!=0||!CreateDirectoryW(root.c_str(),nullptr))throw std::runtime_error("Fresh private QA root required");std::wstring state=root+L"\\state";CreateDirectoryW(state.c_str(),nullptr);eTestHome=state+L"\\";ever::regexTestExecutable(argv[2]);HRESULT ole=OleInitialize(nullptr);INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES};InitCommonControlsEx(&common);glassInit();engineTests(root);windowTests(root);ever::regexShutdown();glassShutdown();if(SUCCEEDED(ole))OleUninitialize();printf("RESULT checks=%d failures=0\n",checks);return 0;}catch(const std::exception&e){printf("FATAL %s Win32=%lu\n",e.what(),GetLastError());ever::regexShutdown();return 1;}
}
