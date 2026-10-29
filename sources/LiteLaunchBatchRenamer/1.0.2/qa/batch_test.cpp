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
    batch::Step clear;clear.kind=batch::Kind::Clear;prefix.rule.text=L"New_";
    check(batch::preview({clear,prefix},items,out,error)&&out[0]==L"New_.JPG","clear then insert keeps original file extension");
    check(batch::preview({clear,prefix},folders,out,error)&&out[0]==L"New_","clear then insert treats dotted folders as whole names");
    std::vector<std::wstring>exts;check(batch::preview({clear},items,out,error,&exts)&&out[0]==L".JPG"&&exts[0]==L".JPG","cleared stem retains extension metadata for empty-name guard");
    batch::Step advanced;advanced.kind=batch::Kind::Advanced;advanced.options={{L"type",L"Serialize"},{L"where",L"replace"},{L"start",L"10"},{L"step",L"-2"},{L"width",L"3"}};
    check(batch::preview({advanced},items,out,error)&&out[0]==L"010.JPG"&&out[1]==L"008.JPG","advanced serialization travels through native worker bridge");
    check(batch::preview({clear,advanced},items,out,error)&&out[0]==L"010.JPG","advanced serialization preserves extension after cleared stem");
    advanced.options={{L"type",L"Insert"},{L"text",L"中文🙂"}};check(batch::preview({advanced},items,out,error)&&out[0]==L"中文🙂PHOTO.JPG","advanced JSON bridge round trips Unicode");
    advanced.options={{L"type",L"RegEx"},{L"find",L"["}};check(!batch::preview({prefix,advanced},items,sentinel,error)&&sentinel[0]==L"unchanged","advanced worker failure preserves existing draft atomically");
}
static void windowTests(const std::wstring&root){
    std::wstring first=root+L"\\A.txt",second=root+L"\\B.txt";{std::ofstream a(first.c_str());a<<"first";std::ofstream b(second.c_str());b<<"second";}
    EverWindow editor;addPath(editor.items,first);addPath(editor.items,second);Scintilla_RegisterClasses(GetModuleHandleW(nullptr));
    WNDCLASSEXW cls={sizeof(cls)};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=EverWindow::proc;cls.lpszClassName=L"LiteLaunch.Batch.PrivateQA";RegisterClassExW(&cls);
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"Batch QA",WS_OVERLAPPEDWINDOW,-20000,-20000,1180,740,nullptr,nullptr,cls.hInstance,&editor);check(window&&editor.editor,"new rule and preview UI creates using production controls");
    check(editor.batchSimple&&(GetWindowLongPtrW(editor.ctl(EverWindow::BR_QDELETE),GWL_STYLE)&WS_VISIBLE)&&!(GetWindowLongPtrW(editor.ctl(EverWindow::BR_RULES),GWL_STYLE)&WS_VISIBLE),"default simple mode exposes delete-clear directly and hides rule queue");
    editor.batchSimple=false;editor.layout();
    check(GetDlgItem(window,EverWindow::BR_RULES)&&GetDlgItem(window,EverWindow::BR_FILES),"ordered rules and original/new preview table present");
    bool layoutValid=true;int visibleIds[]={EverWindow::BR_RULES,EverWindow::BR_FILES,EverWindow::BR_ADD,EverWindow::BR_EDIT,EverWindow::BR_UP,EverWindow::BR_DOWN,EverWindow::BR_DELETE,EverWindow::BR_CLEAR,EverWindow::BR_PREVIEW,EverWindow::BR_LEGACY,EverWindow::BR_SAVE,EverWindow::BR_LOAD,EE_EDITOR,EE_FIND,EE_OK,EE_ADD,EE_FOLDER,EE_READ,EE_OTHER,EE_STATUS};
    for(int scale:{72,96,120,144,192})for(int width:{920,1180})for(int height:{620,740}){editor.dpi=scale;SetWindowPos(window,nullptr,-20000,-20000,MulDiv(width,scale,96),MulDiv(height,scale,96),SWP_NOACTIVATE|SWP_NOZORDER);RECT client;GetClientRect(window,&client);for(int id:visibleIds){RECT box;GetWindowRect(editor.ctl(id),&box);MapWindowPoints(nullptr,window,(POINT*)&box,2);if(box.left<0||box.top<0||box.right>client.right||box.bottom>client.bottom||box.right<=box.left||box.bottom<=box.top)layoutValid=false;}}
    check(layoutValid,"all visible controls stay within compact and wide layouts at 75 to 200 percent scale");
    editor.dpi=96;SetWindowPos(window,nullptr,-20000,-20000,1180,740,SWP_NOACTIVATE|SWP_NOZORDER);
    check(ListView_GetItemCount(editor.ctl(EverWindow::BR_FILES))==2,"preview table includes selected files");
    editor.ruleStack={insert(L"X_")};editor.batchList(0);check(editor.batchPreview(false)&&editor.text()==L"X_A.txt\nX_B.txt","production pipeline updates editable names");
    check(attrs(first)!=INVALID_FILE_ATTRIBUTES&&attrs(root+L"\\X_A.txt")==INVALID_FILE_ATTRIBUTES,"preview never renames real files");
    auto encoded=editor.batchEncode();auto decoded=EverWindow::batchDecode(encoded);check(decoded.size()==1&&decoded[0].rule.text==L"X_","rule set round trip retains parameters");
    auto oldStack=editor.ruleStack;batch::Step rich;rich.kind=batch::Kind::Advanced;rich.options={{L"type",L"Serialize"},{L"step",L"-2"},{L"prefix",L"中文🙂"}};editor.ruleStack={rich};auto richBytes=editor.batchEncode();auto richDecoded=EverWindow::batchDecode(richBytes);check(richDecoded.size()==1&&richDecoded[0].options==rich.options,"version2 presets retain every advanced parameter");
    std::vector<BYTE>legacy;ePut32(legacy,0x31505242);ePut32(legacy,1);ePut32(legacy,0);ePut32(legacy,1);ePut32(legacy,1);ePutText(legacy,L"");ePutText(legacy,ever::serialize(oldStack[0].rule));ePut32(legacy,eHash(legacy,legacy.size()));check(EverWindow::batchDecode(legacy)[0].rule.text==L"X_","version1 presets remain readable");editor.ruleStack=oldStack;
    encoded[8]^=1;bool rejected=false;try{EverWindow::batchDecode(encoded);}catch(...){rejected=true;}check(rejected,"modified rule set rejected before applying");
    ListView_SetCheckState(editor.ctl(EverWindow::BR_RULES),0,FALSE);check(editor.text()==L"A.txt\nB.txt","native checkbox immediately disables one rule");
    ListView_SetCheckState(editor.ctl(EverWindow::BR_RULES),0,TRUE);editor.setText(L"Manual_A.txt\nManual_B.txt");int before=warnings;check(!editor.batchPreview()&&warnings==before+1&&editor.text().find(L"Manual_A")==0,"manual draft is protected from automatic recompute");
    check(editor.batchPreview(false),"explicit preview can recompute from originals");
    editor.toggle(true);batch::Step ext;ext.kind=batch::Kind::Extension;ext.text=L"zip";editor.ruleStack.push_back(ext);check(editor.batchPreview(false)&&editor.extensions[0]==L".zip"&&editor.text()==L"X_A\nX_B","hidden extension tracks extension changes atomically");
    check(editor.execute(),"execution commits composed names only on explicit command");
    check(attrs(root+L"\\X_A.zip")!=INVALID_FILE_ATTRIBUTES&&attrs(first)==INVALID_FILE_ATTRIBUTES,"composed target actually committed within private fixture");
    editor.regret();check(attrs(first)!=INVALID_FILE_ATTRIBUTES&&attrs(second)!=INVALID_FILE_ATTRIBUTES,"existing undo history restores new plugin renames");
    editor.batchSimple=true;editor.hidden=false;editor.rebuild();editor.layout();
    batch::Step simplePrefix=insert(L"old_"),simpleReplace;simpleReplace.rule.mode=ever::Mode::FindReplace;simpleReplace.rule.regex=false;simpleReplace.rule.escapeReplacement=false;simpleReplace.rule.replacements=L"old|new";
    check(editor.batchQuickApply(simplePrefix)&&editor.batchQuickApply(simpleReplace)&&editor.text()==L"new_A.txt\nnew_B.txt","simple tools compose directly on current preview without constructing queue");
    editor.sci(SCI_UNDO);check(editor.text()==L"old_A.txt\nold_B.txt","simple replacement can be undone");editor.sci(SCI_REDO);
    batch::Step clear;clear.kind=batch::Kind::Clear;check(editor.batchQuickApply(clear)&&editor.text()==L"\n"&&eNameLines(editor.text()).size()==2,"clear preserves blank row for each file and hides protected extensions");
    int emptyWarnings=warnings;check(!editor.execute()&&warnings==emptyWarnings+1&&attrs(first)!=INVALID_FILE_ATTRIBUTES,"cleared names cannot commit real file changes");
    editor.sci(SCI_UNDO);check(editor.text()==L"new_A\nnew_B","clear is undoable without losing extension association");editor.sci(SCI_REDO);
    batch::Step sequenceSimple;sequenceSimple.rule.mode=ever::Mode::NamingTemplate;sequenceSimple.rule.namingPattern=L"Report-{num}";sequenceSimple.rule.sequenceStart=L"1";sequenceSimple.rule.sequenceWidth=2;
    check(editor.batchQuickApply(sequenceSimple)&&editor.text()==L"Report-01\nReport-02"&&editor.extensions[0]==L".txt","numbering repopulates cleared names while preserving extensions");
    capturePreview(editor,root+L"\\simple-preview.png");editor.restoreNames();
    std::wstring folderRoot=root+L"\\folders";CreateDirectoryW(folderRoot.c_str(),nullptr);CreateDirectoryW((folderRoot+L"\\First.v1").c_str(),nullptr);CreateDirectoryW((folderRoot+L"\\Second.v2").c_str(),nullptr);
    {std::ofstream a((folderRoot+L"\\First.v1\\keep.txt").c_str());a<<"retained";std::ofstream b((folderRoot+L"\\ignore.txt").c_str());b<<"not a folder";}
    editor.items.clear();std::wstring folderError;check(eAddChildFolders(editor.items,folderRoot,folderError)&&editor.items.size()==2&&editor.items[0].folder&&editor.items[1].folder,"parent directory imports only direct subfolders for batch renaming");
    check(eAddChildFolders(editor.items,folderRoot,folderError)&&editor.items.size()==2,"repeated folder import deduplicates selected paths");
    editor.rebuild();check(editor.batchQuickApply(insert(L"Renamed_"))&&editor.execute(),"simple batch rename commits multiple real folders");
    check(attrs(folderRoot+L"\\Renamed_First.v1\\keep.txt")!=INVALID_FILE_ATTRIBUTES&&attrs(folderRoot+L"\\Renamed_Second.v2")!=INVALID_FILE_ATTRIBUTES,"folder renaming preserves children and dotted names");
    editor.regret();check(attrs(folderRoot+L"\\First.v1\\keep.txt")!=INVALID_FILE_ATTRIBUTES&&attrs(folderRoot+L"\\Second.v2")!=INVALID_FILE_ATTRIBUTES,"undo restores both folder names and their contents");
    std::wstring install=root+L"\\installation";CreateDirectoryW(install.c_str(),nullptr);eTestPresetHome=install+L"\\RenamePresets\\";
    editor.ruleStack={clear,insert(L"Saved_")};check(editor.batchSavePreset(L"我的预设")&&editor.batchPresets()==std::vector<std::wstring>{L"我的预设"},"named preset saves and lists exclusively inside installation directory");
    auto saved=EverWindow::batchDecode(eRead(eTestPresetHome+L"我的预设.llrules"));check(saved.size()==2&&saved[0].kind==batch::Kind::Clear&&saved[1].rule.text==L"Saved_","installation preset round trip includes clear operation");
    check(!editor.batchSavePreset(L"..\\escape")&&attrs(install+L"\\escape.llrules")==INVALID_FILE_ATTRIBUTES,"preset name cannot escape fixed installation directory");
    eTestPresetHome.clear();editor.batchSimple=false;editor.layout();
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
