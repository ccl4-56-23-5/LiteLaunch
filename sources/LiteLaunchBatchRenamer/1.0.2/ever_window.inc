// Included inside llrename after the file identity and transaction primitives.
// UI edits names only. The explicit 确定 command is the sole forward file mutation.
static const wchar_t* eProductName();
static std::string eUtf8(const std::wstring &s) {
    if(s.empty()) return {}; int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);
    if(!n) throw std::runtime_error("Invalid UTF16"); std::string t(n,0); WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),&t[0],n,nullptr,nullptr); return t;
}
static std::wstring eWide(const std::string &s) {
    if(s.empty()) return {}; int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);
    if(!n) throw std::runtime_error("Invalid UTF8"); std::wstring t(n,0); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),&t[0],n); return t;
}
static std::vector<std::wstring> eLines(const std::wstring &s) {
    std::vector<std::wstring> lines; size_t at=0;
    for(;;){size_t end=s.find(L'\n',at);std::wstring line=s.substr(at,end==s.npos?s.npos:end-at);if(!line.empty()&&line.back()==L'\r')line.pop_back();lines.push_back(line);if(end==s.npos)break;at=end+1;}
    return lines;
}
static std::wstring eJoin(const std::vector<std::wstring> &lines) {std::wstring t;for(size_t i=0;i<lines.size();++i){if(i)t+=L'\n';t+=lines[i];}return t;}

/* ---------------------------------------------------------------------------
 * Visual layer.  Presentation only: no control id, command route, persistence
 * or file operation is defined here.  Colours, radii and metrics come from the
 * frozen ui_theme.h tokens; the accent is the runtime panel accent.
 * ------------------------------------------------------------------------- */
#include "ui_theme.h"
#include "glass.h"
#if defined(_LIBCPP_MATH_H) || defined(_MATH_H_)
/* This file is included from inside namespace llrename, so gdiplus.h gets its
   own namespace: it re-includes guiddef.h, whose __mingw_uuidof template is
   declared outside its include guard.  Declared in llrename it would shadow the
   global template and break IID_PPV_ARGS for every interface query in this
   window.  <math.h> is likewise only safe once the translation unit has
   provided the global math declarations (rename.cpp does, and the guard proves
   it).  Without GDI+ the rounded plates fall back to plain GDI. */
#define EVER_GDI_PLUS 1
namespace everGdiPlus {
#include <gdiplus.h>
}
namespace eGp = everGdiPlus::Gdiplus;
#endif

enum { EVER_HEADER=48, EVER_MARGIN=16 };

/* One rounded plate: solid fill plus a 1px border, anti-aliased through GDI+
   and drawn with plain GDI RoundRect when GDI+ is unavailable. */
static void eRoundRect(HDC dc,const RECT &box,int radius,COLORREF fill,COLORREF border){
    bool drawn=false;
#ifdef EVER_GDI_PLUS
    eGp::Graphics *graphics=eGp::Graphics::FromHDC(dc);
    if(graphics){
        graphics->SetSmoothingMode(eGp::SmoothingModeAntiAlias);
        eGp::REAL x=(eGp::REAL)box.left+0.5f,y=(eGp::REAL)box.top+0.5f;
        eGp::REAL width=(eGp::REAL)(box.right-box.left)-1.f,height=(eGp::REAL)(box.bottom-box.top)-1.f;
        eGp::REAL curve=(eGp::REAL)radius;
        if(curve*2.f>height)curve=height/2.f;
        if(curve*2.f>width)curve=width/2.f;
        if(width>0.f&&height>0.f&&curve>0.f){
            eGp::GraphicsPath path;eGp::REAL diameter=curve*2.f;
            path.AddArc(x+width-diameter,y,diameter,diameter,270.f,90.f);
            path.AddArc(x+width-diameter,y+height-diameter,diameter,diameter,0.f,90.f);
            path.AddArc(x,y+height-diameter,diameter,diameter,90.f,90.f);
            path.AddArc(x,y,diameter,diameter,180.f,90.f);
            path.CloseFigure();
            eGp::SolidBrush brush(eGp::Color(255,GetRValue(fill),GetGValue(fill),GetBValue(fill)));
            if(graphics->FillPath(&brush,&path)==eGp::Ok)drawn=true;
            eGp::Pen pen(eGp::Color(255,GetRValue(border),GetGValue(border),GetBValue(border)),1.f);
            if(graphics->DrawPath(&pen,&path)==eGp::Ok)drawn=true;
        }
        delete graphics;
    }
#endif
    if(!drawn){
        HBRUSH brush=CreateSolidBrush(fill);HPEN pen=CreatePen(PS_SOLID,1,border);
        HGDIOBJ oldBrush=SelectObject(dc,brush),oldPen=SelectObject(dc,pen);
        RoundRect(dc,box.left,box.top,box.right,box.bottom,radius*2,radius*2);
        SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);DeleteObject(brush);
    }
}

/* Owner-draw push buttons: white hairline plates, an accent-filled primary
   action with ink picked for contrast, accent-tinted hover and press.  The
   native BUTTON control keeps its id, label, tab order and default behaviour. */
static void eDrawOwnerButton(const DRAWITEMSTRUCT *item,int dpi,COLORREF back,bool primary,bool hot){
    if(!item||item->CtlType!=ODT_BUTTON)return;
    const int scale=dpi>0?dpi:96;
    auto px=[scale](int n){return MulDiv(n,scale,96);};
    RECT box=item->rcItem;
    HBRUSH backdrop=CreateSolidBrush(back);FillRect(item->hDC,&box,backdrop);DeleteObject(backdrop);
    bool disabled=(item->itemState&ODS_DISABLED)!=0,pressed=(item->itemState&ODS_SELECTED)!=0;
    COLORREF accent=glassAccentColor(),fill=UI_SURFACE,border=UI_LINE,ink=UI_INK;
    if(primary){fill=accent;border=accent;ink=uiInkOn(accent);}
    else if(hot&&!disabled){fill=uiAccentWash(accent,12);border=uiBlend(UI_LINE,accent,55);}
    if(pressed&&!disabled)fill=primary?uiDarken(accent,10):uiAccentWash(accent,22);
    if(disabled){fill=UI_SURFACE_ALT;border=UI_LINE;ink=UI_FAINT;}
    eRoundRect(item->hDC,box,px(UI_R_BUTTON),fill,border);
    if((item->itemState&ODS_FOCUS)&&!disabled){
        RECT focus=box;InflateRect(&focus,-px(3),-px(3));
        eRoundRect(item->hDC,focus,px(UI_R_BUTTON-3),fill,primary?uiLighten(accent,40):accent);
    }
    WCHAR label[128]=L"";GetWindowTextW(item->hwndItem,label,128);
    HFONT font=(HFONT)SendMessageW(item->hwndItem,WM_GETFONT,0,0);
    SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,ink);
    if(font)SelectObject(item->hDC,font);
    if(pressed)OffsetRect(&box,0,px(1));
    DrawTextW(item->hDC,label,-1,&box,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_HIDEPREFIX);
}

/* Theme-aware buttons do not reliably report ODS_HOTLIGHT for BS_OWNERDRAW, so
   the hot control id is tracked in the owning window and read back when the
   button asks to be painted. */
static LRESULT CALLBACK eHoverProc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR id,DWORD_PTR ref){
    int *hot=(int*)ref;
    if(m==WM_MOUSEMOVE){
        TRACKMOUSEEVENT track={sizeof(track)};track.dwFlags=TME_LEAVE;track.hwndTrack=w;TrackMouseEvent(&track);
        if(hot&&*hot!=GetDlgCtrlID(w)){*hot=GetDlgCtrlID(w);InvalidateRect(w,nullptr,FALSE);}
    }else if(m==WM_MOUSELEAVE){
        if(hot&&*hot==GetDlgCtrlID(w)){*hot=0;InvalidateRect(w,nullptr,FALSE);}
    }else if(m==WM_NCDESTROY)RemoveWindowSubclass(w,eHoverProc,id);
    return DefSubclassProc(w,m,a,b);
}
static void eTrackHover(HWND button,int *hot){if(button)SetWindowSubclass(button,eHoverProc,1,(DWORD_PTR)hot);}
#ifdef LITELAUNCH_EVER_TESTING
static std::wstring eTestHome;
static std::wstring eTestPresetHome;
static std::vector<std::wstring> eTestPaths;
#endif
static std::wstring eHome() {
#ifdef LITELAUNCH_EVER_TESTING
    if(!eTestHome.empty())return eTestHome;
#endif
    wchar_t base[MAX_PATH]={}; if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,base)))throw std::runtime_error("Profile unavailable");
    std::wstring root=std::wstring(base)+L"\\LiteLaunch";CreateDirectoryW(root.c_str(),nullptr);root+=L"\\Rename";if(!CreateDirectoryW(root.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)throw std::runtime_error("Rename profile unavailable");return root+L"\\";
}
static std::wstring eSetting(const wchar_t *key,const wchar_t *fallback=L"") {std::vector<wchar_t>b(32768);GetPrivateProfileStringW(L"EVER",key,fallback,b.data(),(DWORD)b.size(),(eHome()+L"settings.ini").c_str());return b.data();}
static void eSaveSetting(const wchar_t *key,const std::wstring &value);
static bool eAtomic(const std::wstring &path,const std::vector<BYTE> &bytes) {
    std::wstring temp=path+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;DWORD done=0;bool ok=WriteFile(h,bytes.data(),(DWORD)bytes.size(),&done,nullptr)&&done==bytes.size()&&FlushFileBuffers(h);CloseHandle(h);
    if(ok)ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;if(!ok)DeleteFileW(temp.c_str());return ok;
}
static std::vector<BYTE> eRead(const std::wstring &path,DWORD limit=16*1024*1024) {
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE){DWORD error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND)return {};throw std::runtime_error("Cannot read state");}
    LARGE_INTEGER n;DWORD got=0;std::vector<BYTE>b;if(GetFileSizeEx(h,&n)&&n.QuadPart>0&&n.QuadPart<=limit){b.resize((size_t)n.QuadPart);if(!ReadFile(h,b.data(),(DWORD)b.size(),&got,nullptr)||got!=b.size())b.clear();}CloseHandle(h);if(b.empty())throw std::runtime_error("Invalid or unreadable state");return b;
}
static void eSaveSetting(const wchar_t *key,const std::wstring &value) {
    std::wstring path=eHome()+L"settings.ini";auto bytes=eRead(path,1024*1024);
    if(bytes.size()<2||bytes[0]!=0xFF||bytes[1]!=0xFE){
        std::wstring old;if(!bytes.empty()){UINT page=CP_ACP;size_t offset=0;if(bytes.size()>=3&&bytes[0]==0xEF&&bytes[1]==0xBB&&bytes[2]==0xBF){page=CP_UTF8;offset=3;}int n=MultiByteToWideChar(page,0,(const char*)bytes.data()+offset,(int)(bytes.size()-offset),nullptr,0);old.resize(n);if(n)MultiByteToWideChar(page,0,(const char*)bytes.data()+offset,(int)(bytes.size()-offset),&old[0],n);}
        std::vector<BYTE>unicode={0xFF,0xFE};const BYTE*data=(const BYTE*)old.data();unicode.insert(unicode.end(),data,data+old.size()*sizeof(wchar_t));WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str());if(!eAtomic(path,unicode))throw std::runtime_error("Unicode settings initialization failed");WritePrivateProfileStringW(nullptr,nullptr,nullptr,path.c_str());
    }
    if(!WritePrivateProfileStringW(L"EVER",key,value.c_str(),path.c_str()))throw std::runtime_error("Settings write failed");
}
static void ePut32(std::vector<BYTE>&b,DWORD n){for(int i=0;i<4;++i)b.push_back((BYTE)(n>>(i*8)));}
static DWORD eGet32(const std::vector<BYTE>&b,size_t &at){if(at+4>b.size())throw std::runtime_error("Truncated state");DWORD n=0;for(int i=0;i<4;++i)n|=(DWORD)b[at++]<<(i*8);return n;}
static DWORD eHash(const std::vector<BYTE>&b,size_t n){DWORD h=2166136261u;for(size_t i=0;i<n;++i)h=(h^b[i])*16777619u;return h;}
static void ePutText(std::vector<BYTE>&b,const std::wstring&s){std::string t=eUtf8(s);ePut32(b,(DWORD)t.size());b.insert(b.end(),t.begin(),t.end());}
static std::wstring eGetText(const std::vector<BYTE>&b,size_t &at){DWORD n=eGet32(b,at);if(n>1024*1024||at+n>b.size())throw std::runtime_error("Invalid state string");std::string t((const char*)b.data()+at,n);at+=n;return eWide(t);}
static std::vector<Move> eLoadHistory() {
    std::vector<Move> result;auto b=eRead(eHome()+L"undo.bin");if(b.empty())return result;size_t at=0;
    if(b.size()<12||eGet32(b,at)!=0x31485245)throw std::runtime_error("Invalid undo history");DWORD n=eGet32(b,at);if(n>100000)throw std::runtime_error("Invalid history count");
    for(DWORD i=0;i<n;++i){Move m;m.before=eGetText(b,at);m.after=eGetText(b,at);m.identity.volume=eGet32(b,at);m.identity.high=eGet32(b,at);m.identity.low=eGet32(b,at);if(parent(m.before)!=parent(m.after))throw std::runtime_error("Invalid history path");result.push_back(m);}
    size_t payload=at;DWORD hash=eGet32(b,at);if(at!=b.size()||hash!=eHash(b,payload))throw std::runtime_error("Damaged undo history");return result;
}
static bool eStoreHistory(const std::vector<Move>&moves) {std::vector<BYTE>b;ePut32(b,0x31485245);ePut32(b,(DWORD)moves.size());for(const auto&m:moves){ePutText(b,m.before);ePutText(b,m.after);ePut32(b,m.identity.volume);ePut32(b,m.identity.high);ePut32(b,m.identity.low);}ePut32(b,eHash(b,b.size()));return eAtomic(eHome()+L"undo.bin",b);}
static std::vector<Move> eRegretSuccess(const std::vector<Move>&pending,const std::vector<Move>&restored) {
    // A retry that restored a transaction's temporary file must never offer that
    // temporary name as the next target. Finish recovery with no redo record.
    for(const auto&m:pending)if(fileName(m.after).compare(0,13,L".~LiteLaunch-")==0)return {};
    return restored;
}
struct ETemplate {std::wstring name;ever::Rule rule;};
static std::vector<ETemplate> eTemplates() {std::vector<ETemplate> ts;auto b=eRead(eHome()+L"templates.bin");if(b.empty())return ts;size_t at=0;if(eGet32(b,at)!=0x31545245)throw std::runtime_error("Invalid templates");DWORD n=eGet32(b,at);if(n>10000)throw std::runtime_error("Too many templates");for(DWORD i=0;i<n;++i){ETemplate t;t.name=eGetText(b,at);std::wstring encoded=eGetText(b,at),err;if(!ever::deserialize(encoded,t.rule,err))throw std::runtime_error("Invalid template rule");ts.push_back(t);}size_t payload=at;DWORD hash=eGet32(b,at);if(at!=b.size()||hash!=eHash(b,payload))throw std::runtime_error("Damaged templates");return ts;}
static bool eStoreTemplates(const std::vector<ETemplate>&ts) {std::vector<BYTE>b;ePut32(b,0x31545245);ePut32(b,(DWORD)ts.size());for(const auto&t:ts){ePutText(b,t.name);ePutText(b,ever::serialize(t.rule));}ePut32(b,eHash(b,b.size()));return eAtomic(eHome()+L"templates.bin",b);}

// A small modal form shared by the original parameter panels, settings and templates.
struct EField {int id;std::wstring label,value;int type=0;std::vector<std::wstring> choices;bool visible=true;std::wstring help;};
struct EForm {
    HWND window=nullptr,owner=nullptr,tooltip=nullptr;HFONT font=nullptr;int dpi=96;bool accepted=false;std::wstring title;
    HBRUSH background=nullptr;int hot=0;
    std::vector<EField> fields;std::function<void(EForm&)> changed;std::function<void(EForm&)> extra;std::wstring extraLabel;
    int px(int n)const{return MulDiv(n,dpi,96);} HWND ctl(int id)const{return GetDlgItem(window,id);}
    std::wstring get(int id)const{HWND h=ctl(id);for(const auto&field:fields)if(field.id==id&&field.type>=3){int i=(int)SendMessageW(h,CB_GETCURSEL,0,0);if(i>=0){int n=(int)SendMessageW(h,CB_GETLBTEXTLEN,i,0);std::wstring s(n+1,0);SendMessageW(h,CB_GETLBTEXT,i,(LPARAM)&s[0]);s.resize(n);return s;}}int n=GetWindowTextLengthW(h);std::wstring s(n+1,0);GetWindowTextW(h,&s[0],n+1);s.resize(n);return s;}
    int num(int id)const{return _wtoi(get(id).c_str());} bool check(int id)const{return SendMessageW(ctl(id),BM_GETCHECK,0,0)==BST_CHECKED;}
    void set(int id,const std::wstring &s){SetWindowTextW(ctl(id),s.c_str());}
    int scrollY=0,scrollMax=0;
    void layout(bool resize=true){
        HDC dc=GetDC(window);HGDIOBJ old=SelectObject(dc,font);int y=px(EVER_MARGIN);
        struct Row{EField*field;int y,height,labelHeight;};std::vector<Row>rows;
        for(auto&f:fields){ShowWindow(ctl(f.id+1000),f.visible?SW_SHOW:SW_HIDE);ShowWindow(ctl(f.id),f.visible?SW_SHOW:SW_HIDE);if(!f.visible)continue;
            RECT text={0,0,px(194),0};DrawTextW(dc,f.label.c_str(),-1,&text,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
            int height=std::max(px(f.type==2?112:30),(int)text.bottom+px(8));rows.push_back({&f,y,height,(int)text.bottom});y+=height+px(10);
        }SelectObject(dc,old);ReleaseDC(window,dc);
        int footer=y+px(6),content=footer+px(54);MONITORINFO mi={sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&mi);
        RECT frame={0,0,px(596),0};AdjustWindowRectEx(&frame,WS_CAPTION|WS_SYSMENU|WS_VSCROLL,FALSE,WS_EX_DLGMODALFRAME);
        int viewport=std::min(content,(int)(mi.rcWork.bottom-mi.rcWork.top-frame.bottom+frame.top)-px(24));viewport=std::max(px(100),viewport);
        scrollMax=std::max(0,content-viewport);scrollY=std::min(scrollY,scrollMax);SCROLLINFO si={sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,content-1,(UINT)viewport,scrollY,0};SetScrollInfo(window,SB_VERT,&si,TRUE);
        for(const auto&row:rows){auto&f=*row.field;int top=row.y-scrollY;MoveWindow(ctl(f.id+1000),px(20),top+px(4),px(194),row.labelHeight+px(2),TRUE);MoveWindow(ctl(f.id),px(222),top,px(354),px(f.type>=3?240:f.type==2?112:30),TRUE);}
        int buttons=footer-scrollY;MoveWindow(ctl(IDOK),px(360),buttons,px(104),px(34),TRUE);MoveWindow(ctl(IDCANCEL),px(472),buttons,px(104),px(34),TRUE);if(extra)MoveWindow(ctl(990),px(20),buttons,px(190),px(34),TRUE);
        if(resize){RECT r={0,0,px(596),viewport};AdjustWindowRectEx(&r,WS_CAPTION|WS_SYSMENU|WS_VSCROLL,FALSE,WS_EX_DLGMODALFRAME);RECT at;GetWindowRect(window,&at);int x=std::max((int)mi.rcWork.left,std::min((int)at.left,(int)(mi.rcWork.right-(r.right-r.left)))),top=std::max((int)mi.rcWork.top,std::min((int)at.top,(int)(mi.rcWork.bottom-(r.bottom-r.top))));SetWindowPos(window,nullptr,x,top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);}
        InvalidateRect(window,nullptr,TRUE);
    }
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){EForm*f=(EForm*)GetWindowLongPtrW(w,GWLP_USERDATA);if(m==WM_NCCREATE){f=(EForm*)((CREATESTRUCTW*)b)->lpCreateParams;f->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)f);}if(!f)return DefWindowProcW(w,m,a,b);try{
        if(m==WM_CREATE){f->background=CreateSolidBrush(UI_CANVAS);f->tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP,0,0,0,0,w,nullptr,GetModuleHandleW(nullptr),nullptr);SendMessageW(f->tooltip,TTM_SETMAXTIPWIDTH,0,f->px(420));f->font=CreateFontW(-f->px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");for(auto&field:f->fields){CreateWindowExW(0,L"STATIC",field.label.c_str(),WS_CHILD|WS_VISIBLE,0,0,0,0,w,(HMENU)(INT_PTR)(field.id+1000),GetModuleHandleW(nullptr),nullptr);const wchar_t*cls=field.type==1?L"BUTTON":field.type>=3?L"COMBOBOX":L"EDIT";DWORD style=WS_CHILD|WS_VISIBLE|WS_TABSTOP|(field.type==1?BS_AUTOCHECKBOX:field.type>=3?(field.type==4?CBS_DROPDOWNLIST:CBS_DROPDOWN)|WS_VSCROLL:field.type==2?ES_MULTILINE|ES_WANTRETURN|WS_VSCROLL|ES_AUTOVSCROLL:ES_AUTOHSCROLL);HWND h=CreateWindowExW(field.type==1?0:WS_EX_CLIENTEDGE,cls,field.type==1?L"":field.value.c_str(),style,0,0,0,0,w,(HMENU)(INT_PTR)field.id,GetModuleHandleW(nullptr),nullptr);if(field.type==1)SendMessageW(h,BM_SETCHECK,field.value==L"1"?BST_CHECKED:BST_UNCHECKED,0);if(field.type>=3){int selected=-1;for(size_t i=0;i<field.choices.size();++i){SendMessageW(h,CB_ADDSTRING,0,(LPARAM)field.choices[i].c_str());if(field.choices[i]==field.value)selected=(int)i;}if(selected>=0)SendMessageW(h,CB_SETCURSEL,selected,0);else if(field.type==3)SetWindowTextW(h,field.value.c_str());}if(!field.help.empty()){TOOLINFOW tip={sizeof(tip)};tip.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tip.hwnd=w;tip.uId=(UINT_PTR)h;tip.lpszText=(LPWSTR)field.help.c_str();SendMessageW(f->tooltip,TTM_ADDTOOLW,0,(LPARAM)&tip);}SendMessageW(h,WM_SETFONT,(WPARAM)f->font,TRUE);SendMessageW(GetDlgItem(w,field.id+1000),WM_SETFONT,(WPARAM)f->font,TRUE);}
        for(int id:{IDOK,IDCANCEL,990}){if(id==990&&!f->extra)continue;HWND h=CreateWindowExW(0,L"BUTTON",id==IDOK?L"确定":id==IDCANCEL?L"取消":f->extraLabel.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,w,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,(WPARAM)f->font,TRUE);eTrackHover(h,&f->hot);}if(f->changed)f->changed(*f);f->layout();return 0;}
        if(m==WM_ERASEBKGND)return 1;
        if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT client;GetClientRect(w,&client);if(f->background)FillRect(dc,&client,f->background);EndPaint(w,&ps);return 0;}
        if(m==WM_CTLCOLORSTATIC||m==WM_CTLCOLORBTN){HDC dc=(HDC)a;SetBkMode(dc,TRANSPARENT);SetTextColor(dc,m==WM_CTLCOLORSTATIC?UI_INK_SOFT:UI_INK);return (LRESULT)(f->background?f->background:(HBRUSH)GetStockObject(WHITE_BRUSH));}
        if(m==WM_CTLCOLOREDIT||m==WM_CTLCOLORLISTBOX){HDC dc=(HDC)a;SetBkColor(dc,UI_SURFACE);SetTextColor(dc,UI_INK);return (LRESULT)GetStockObject(WHITE_BRUSH);}
        if(m==WM_DRAWITEM){DRAWITEMSTRUCT *item=(DRAWITEMSTRUCT*)b;if(item->CtlType==ODT_BUTTON){eDrawOwnerButton(item,f->dpi,UI_CANVAS,item->CtlID==IDOK,(item->itemState&ODS_HOTLIGHT)!=0||f->hot==(int)item->CtlID);return TRUE;}return FALSE;}
        if(m==WM_VSCROLL||m==WM_MOUSEWHEEL){int next=f->scrollY;if(m==WM_MOUSEWHEEL)next-=GET_WHEEL_DELTA_WPARAM(a)/WHEEL_DELTA*f->px(60);else{SCROLLINFO si={sizeof(si),SIF_TRACKPOS};GetScrollInfo(w,SB_VERT,&si);switch(LOWORD(a)){case SB_LINEUP:next-=f->px(30);break;case SB_LINEDOWN:next+=f->px(30);break;case SB_PAGEUP:next-=f->px(160);break;case SB_PAGEDOWN:next+=f->px(160);break;case SB_THUMBTRACK:next=si.nTrackPos;break;}}f->scrollY=std::max(0,std::min(next,f->scrollMax));f->layout(false);return 0;}
        if(m==WM_COMMAND){int id=LOWORD(a);if(id==IDOK){for(auto&field:f->fields)field.value=field.type==1?(f->check(field.id)?L"1":L"0"):f->get(field.id);f->accepted=true;DestroyWindow(w);}else if(id==IDCANCEL)DestroyWindow(w);else if(id==990&&f->extra)f->extra(*f);else if(f->changed&&(HIWORD(a)==CBN_SELCHANGE||HIWORD(a)==CBN_EDITCHANGE||HIWORD(a)==BN_CLICKED)){f->changed(*f);f->layout();}return 0;}if(m==WM_CLOSE){DestroyWindow(w);return 0;}if(m==WM_NCDESTROY){if(f->font)DeleteObject(f->font);f->font=nullptr;if(f->background)DeleteObject(f->background);f->background=nullptr;f->window=nullptr;}
    }catch(const std::exception&){MessageBoxW(w,L"参数窗口遇到错误，未执行文件操作。",eProductName(),MB_ICONERROR);}return DefWindowProcW(w,m,a,b);}
    bool run(HWND parentWindow,int scale,const std::wstring &caption){owner=parentWindow;dpi=scale;title=caption;WNDCLASSEXW wc={sizeof(wc)};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=nullptr;wc.lpszClassName=L"LiteLaunch.Ever.Form";RegisterClassExW(&wc);RECT work={};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);bool enabled=owner&&IsWindowEnabled(owner);if(enabled)EnableWindow(owner,FALSE);HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,title.c_str(),WS_CAPTION|WS_SYSMENU|WS_VSCROLL,(work.left+work.right-px(540))/2,(work.top+work.bottom-px(450))/2,px(540),px(450),owner,nullptr,wc.hInstance,this);if(h){ShowWindow(h,SW_SHOW);if(!fields.empty())SetFocus(ctl(fields.front().id));MSG msg;int got=1;while(IsWindow(h)&&(got=GetMessageW(&msg,nullptr,0,0))>0){if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(got==0){if(IsWindow(h))DestroyWindow(h);PostQuitMessage((int)msg.wParam);}}if(enabled&&IsWindow(owner)){EnableWindow(owner,TRUE);SetActiveWindow(owner);}return accepted;}
    std::wstring value(int id)const{for(const auto&f:fields)if(f.id==id)return f.value;return L"";}
};
static bool eAsk(HWND owner,int dpi,const wchar_t *title,const wchar_t *label,std::wstring &value,bool multi=false){EForm f;f.fields.push_back({101,label,value,multi?2:0});if(!f.run(owner,dpi,title))return false;value=f.value(101);return true;}
static void eCopy(HWND w,const std::wstring &text){if(!OpenClipboard(w))return;HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,(text.size()+1)*sizeof(wchar_t));if(h){void*p=GlobalLock(h);if(p){memcpy(p,text.c_str(),(text.size()+1)*sizeof(wchar_t));GlobalUnlock(h);EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,h))GlobalFree(h);}else GlobalFree(h);}CloseClipboard();}
static bool ePickPath(HWND owner,bool save,std::wstring &path,const wchar_t *filter=L"EVER模板备份\0*.ever\0所有文件\0*.*\0"){wchar_t b[32768]={};if(path.size()<32768)wcscpy(b,path.c_str());OPENFILENAMEW p={sizeof(p)};p.hwndOwner=owner;p.lpstrFile=b;p.nMaxFile=32768;p.lpstrFilter=filter;p.lpstrDefExt=L"ever";p.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);if(!(save?GetSaveFileNameW(&p):GetOpenFileNameW(&p)))return false;path=b;return true;}

enum { EE_EDITOR=400,EE_STATUS,EE_OK,EE_FIND,EE_RULE,EE_OTHER,EE_EXT,EE_LINK,EE_ADD,EE_FOLDER,EE_READ,EE_ZOOMIN,EE_ZOOMOUT,EE_COPY,EE_SPEAK,EE_SEARCH,EE_UNDO,EE_REDO,
    EE_RESTORE,EE_PIN,EE_HIGHLIGHT,EE_CUT,EE_PASTE,EE_SELECTALL,
    EE_NUMBER=500,EE_CHANGEEXT,EE_BEGINNING,EE_PART,EE_DIRECTORY,EE_COUNT,EE_HIDE,EE_SHOW,EE_REGRET,EE_TEMPLATES,EE_SETTINGS,EE_LINKSET,EE_SORT,EE_BACKUP,EE_IMPORT,EE_NAMING=650,EE_TEMPLATEBASE=700,EE_LINKBASE=12000 };
static const wchar_t *eSortValues[]={L"Default",L"Origin",L"FileName",L"FileNameNature",L"FileSizeAsc",L"FileSizeDesc",L"CreationTimeDesc",L"CreationTimeAsc",L"LastAccessTimeDesc",L"LastAccessTimeAsc",L"LastWriteTimeDesc",L"LastWriteTimeAsc"};
static std::vector<std::wstring> eSortLabels(){return {L"默认（文件名自然排序）",L"原始（系统返回顺序）",L"文件名（字母顺序）",L"文件名（自然顺序）",L"文件大小（从小到大）",L"文件大小（从大到小）",L"创建时间（从新到旧）",L"创建时间（从旧到新）",L"最后访问时间（从晚到早）",L"最后访问时间（从早到晚）",L"最后写入时间（从晚到早）",L"最后写入时间（从早到晚）"};}
static std::wstring eSortLabel(){auto labels=eSortLabels();std::wstring mode=eSetting(L"Sort",L"Origin");for(int i=0;i<12;++i)if(mode==eSortValues[i])return labels[i];return labels[1];}
static bool eSourceSelection(HWND source,std::vector<Entry>&items) {
    if(readSelection(source,items))return true;if(!source||!IsWindow(source))return false;HWND root=GetAncestor(source,GA_ROOT);wchar_t cls[100]={};GetClassNameW(root,cls,100);
    if(_wcsicmp(cls,L"EVERYTHING")&&_wcsicmp(cls,L"TTOTAL_CMD"))return false;
    Com<IDataObject> backup;if(FAILED(OleGetClipboard(backup.out())))return false;DWORD sequence=GetClipboardSequenceNumber();SetForegroundWindow(root);if(GetForegroundWindow()!=root)return false;
    INPUT keys[4]={};for(auto&key:keys)key.type=INPUT_KEYBOARD;keys[0].ki.wVk=VK_CONTROL;keys[1].ki.wVk='C';keys[2].ki.wVk='C';keys[2].ki.dwFlags=KEYEVENTF_KEYUP;keys[3].ki.wVk=VK_CONTROL;keys[3].ki.dwFlags=KEYEVENTF_KEYUP;
    if(SendInput(4,keys,sizeof(INPUT))!=4)return false;ULONGLONG end=GetTickCount64()+std::max(200,std::min(9000,_wtoi(eSetting(L"ClipboardWait",L"1000").c_str())));
    while(GetClipboardSequenceNumber()==sequence&&GetTickCount64()<end)Sleep(10);bool copied=GetClipboardSequenceNumber()!=sequence;size_t before=items.size();
    if(copied&&OpenClipboard(root)){HDROP drop=(HDROP)GetClipboardData(CF_HDROP);if(drop){UINT n=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);for(UINT i=0;i<n;++i){UINT len=DragQueryFileW(drop,i,nullptr,0);std::wstring path(len+1,0);DragQueryFileW(drop,i,&path[0],len+1);path.resize(len);addPath(items,path);}}else{HANDLE text=GetClipboardData(CF_UNICODETEXT);if(text){const wchar_t*value=(const wchar_t*)GlobalLock(text);if(value){auto lines=eLines(value);for(auto path:lines){if(path.size()>1&&path.front()==L'"'&&path.back()==L'"')path=path.substr(1,path.size()-2);addPath(items,path);}GlobalUnlock(text);}}}CloseClipboard();}
    OleSetClipboard(backup.p);OleFlushClipboard();return items.size()!=before;
}
static bool eResolveNames(const std::vector<Entry>&items,std::vector<std::wstring>&names,std::wstring&error,bool&changed) {
    if(items.size()!=names.size()){error=L"名称行数不一致。";return false;}changed=false;std::vector<std::wstring>reserved;unsigned long long number=1;
    for(size_t i=0;i<items.size();++i){std::wstring reason;if(!validName(names[i],reason)){error=names[i]+L"："+reason;return false;}if(names[i]==items[i].name)reserved.push_back(items[i].path);}
    for(size_t i=0;i<items.size();++i){if(names[i]==items[i].name)continue;std::wstring base=names[i],ext;size_t dot=base.find_last_of(L'.');if(!items[i].folder&&dot!=base.npos&&dot>0){ext=base.substr(dot);base.resize(dot);}std::wstring candidate=names[i];
        for(;;){std::wstring path=parent(items[i].path)+candidate;bool busy=false;for(const auto&p:reserved)if(same(p,path)){busy=true;break;}
            if(!busy&&attrs(path)!=INVALID_FILE_ATTRIBUTES){bool selected=false;for(const auto&entry:items)if(same(entry.path,path)){selected=true;break;}busy=!selected;}
            if(!busy){reserved.push_back(path);break;}if(number>100000){error=L"无法找到不冲突的名称。";return false;}candidate=base+L"_"+std::to_wstring(number++)+ext;std::wstring reason;if(!validName(candidate,reason)){error=reason;return false;}
        }
        if(candidate!=names[i]){names[i]=candidate;changed=true;}
    }
    return true;
}
static std::vector<std::wstring> eNameLines(const std::wstring&text){auto lines=eLines(text);
#ifndef LITELAUNCH_BATCH_PLUGIN
lines.erase(std::remove(lines.begin(),lines.end(),L""),lines.end());
#endif
return lines;}
static bool eInteger(const std::wstring&s,int &n){if(s.empty())return false;wchar_t*end=nullptr;errno=0;long long value=wcstoll(s.c_str(),&end,10);if(errno||!end||*end||value<INT_MIN||value>INT_MAX)return false;n=(int)value;return true;}
static std::wstring eClipboardText(HWND window){std::wstring result;if(OpenClipboard(window)){HANDLE h=GetClipboardData(CF_UNICODETEXT);if(h){const wchar_t*p=(const wchar_t*)GlobalLock(h);if(p){result=p;GlobalUnlock(h);}}CloseClipboard();}return result;}
struct EExtensionUndo {bool before=false,after=false;std::vector<std::wstring>oldExtensions,newExtensions;};
static std::wstring ePresetHome(){
#ifdef LITELAUNCH_EVER_TESTING
    if(!eTestPresetHome.empty())return eTestPresetHome;
#endif
    wchar_t exe[32768]={};DWORD length=GetEnvironmentVariableW(L"LITELAUNCH_HOST_EXE",exe,32768);
    if(!length||length>=32768||attrs(exe)==INVALID_FILE_ATTRIBUTES)GetModuleFileNameW(nullptr,exe,32768);
    return parent(exe)+L"RenamePresets\\";
}
static bool eAddChildFolders(std::vector<Entry>&items,const std::wstring&root,std::wstring&error){
    WIN32_FIND_DATAW data={};HANDLE search=FindFirstFileW(nativePath(root+L"\\*").c_str(),&data);
    if(search==INVALID_HANDLE_VALUE){if(GetLastError()==ERROR_FILE_NOT_FOUND)return true;error=winError(GetLastError());return false;}
    do{if((data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&wcscmp(data.cFileName,L".")&&wcscmp(data.cFileName,L".."))addPath(items,root+L"\\"+data.cFileName);}while(FindNextFileW(search,&data));
    DWORD last=GetLastError();FindClose(search);if(last!=ERROR_NO_MORE_FILES){error=winError(last);return false;}return true;
}
static const wchar_t* eProductName(){
#ifdef LITELAUNCH_BATCH_PLUGIN
    return L"批量重命名";
#else
    return L"EVER重命名";
#endif
}
static const wchar_t* eDefaultFontSize(){
#ifdef LITELAUNCH_BATCH_PLUGIN
    return L"11";
#else
    return L"14";
#endif
}
static HWND eCurrentWindow=nullptr;
struct EverWindow {
    HWND window=nullptr,owner=nullptr,source=nullptr,editor=nullptr;HFONT font=nullptr,titleFont=nullptr;HBRUSH background=nullptr;int dpi=96;bool hidden=false,closing=false;std::vector<Entry> items;std::vector<std::wstring> extensions;std::vector<Move> history;std::vector<EExtensionUndo> extensionUndo;Com<IDispatch> voice;
    int hot=0;
    int px(int n)const{return MulDiv(n,dpi,96);} HWND ctl(int id)const{return GetDlgItem(window,id);} LRESULT sci(UINT m,WPARAM a=0,LPARAM b=0)const{return SendMessageW(editor,m,a,b);}
    void status(const std::wstring&s){SetWindowTextW(ctl(EE_STATUS),s.c_str());}
    void warn(const std::wstring&s){MessageBoxW(window,s.c_str(),eProductName(),MB_ICONWARNING);}
    std::wstring text()const{int n=(int)sci(SCI_GETTEXTLENGTH);std::string s(n+1,0);sci(SCI_GETTEXT,n+1,(LPARAM)&s[0]);s.resize(n);return eWide(s);}
    std::wstring selectedText()const{int n=(int)sci(SCI_GETSELTEXT);std::string s(n>0?n:1,0);sci(SCI_GETSELTEXT,0,(LPARAM)&s[0]);if(!s.empty()&&s.back()==0)s.pop_back();return eWide(s);}
    void setText(const std::wstring &s,bool undoable=true){std::string t=eUtf8(s);if(undoable){sci(SCI_BEGINUNDOACTION);sci(SCI_SETTARGETSTART,0);sci(SCI_SETTARGETEND,sci(SCI_GETTEXTLENGTH));sci(SCI_REPLACETARGET,t.size(),(LPARAM)t.data());sci(SCI_ENDUNDOACTION);}else{sci(SCI_SETTEXT,0,(LPARAM)t.c_str());sci(SCI_EMPTYUNDOBUFFER);extensionUndo.clear();}update();}
    void textAndExtensionMode(const std::vector<std::wstring>&names,bool mode,const std::vector<std::wstring>&exts){EExtensionUndo action;action.before=hidden;action.after=mode;action.oldExtensions=extensions;action.newExtensions=exts;extensionUndo.push_back(action);sci(SCI_BEGINUNDOACTION);sci(SCI_ADDUNDOACTION,extensionUndo.size(),0);hidden=mode;extensions=exts;setText(eJoin(names));sci(SCI_ENDUNDOACTION);update();}
    void extensionNotification(SCNotification*notification){if(!(notification->modificationType&SC_MOD_CONTAINER)||notification->token<=0||(size_t)notification->token>extensionUndo.size())return;const auto&action=extensionUndo[notification->token-1];bool undo=(notification->modificationType&SC_PERFORMED_UNDO)!=0;hidden=undo?action.before:action.after;extensions=undo?action.oldExtensions:action.newExtensions;update();}
    void restoreNames(){std::vector<std::wstring>ns,exts;for(const auto&e:items){std::wstring ext=extension(e);exts.push_back(ext);ns.push_back(hidden?e.name.substr(0,e.name.size()-ext.size()):e.name);}textAndExtensionMode(ns,hidden,exts);}
    bool protectDraft(){if(items.empty())return true;std::vector<std::wstring>current;for(size_t i=0;i<items.size();++i){std::wstring name=items[i].name;if(hidden)name.resize(name.size()-extensions[i].size());current.push_back(name);}if(eJoin(current)==text())return true;return MessageBoxW(window,L"当前名称尚未提交。继续读取或添加项目会重新载入名称，是否继续？",eProductName(),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)==IDYES;}
    bool names(std::vector<std::wstring>&out){out=eNameLines(text());if(items.empty()&&out.size()==1&&out[0].empty()){out.clear();return true;}if(out.size()!=items.size()){warn(L"名称行数与项目数不一致。每行对应一个项目，请保留原行数和顺序。");return false;}return true;}
    static std::wstring extension(const Entry&e){size_t dot=e.name.find_last_of(L'.');return !e.folder&&dot!=e.name.npos&&dot>0?e.name.substr(dot):L"";}
    void rebuild(){std::vector<std::wstring>ns;extensions.clear();for(const auto&e:items){std::wstring ext=extension(e);extensions.push_back(ext);ns.push_back(hidden?e.name.substr(0,e.name.size()-ext.size()):e.name);}setText(eJoin(ns),false);if(items.empty())SetWindowTextW(window,eProductName());else SetWindowTextW(window,(std::wstring(eProductName())+L"    "+parent(items.front().path)).c_str());
#ifdef LITELAUNCH_BATCH_PLUGIN
        batchPreview(false);
#endif
    }
    void update(){if(!editor)return;auto ns=eNameLines(text());bool valid=ns.size()==items.size()&&!items.empty();EnableWindow(ctl(EE_OK),valid);status(std::to_wstring(items.size())+L"个项目 · "+std::to_wstring(ns.size())+L"行 · "+(hidden?L"扩展名已隐藏":L"扩展名已显示"));
#ifdef LITELAUNCH_BATCH_PLUGIN
        SetTimer(window,8201,120,nullptr);
#endif
    }
    void toggle(bool hide){if(hide==hidden)return;std::vector<std::wstring>ns,exts=extensions;if(!names(ns))return;if(hide){exts.clear();for(size_t i=0;i<ns.size();++i){Entry e=items[i];e.name=ns[i];std::wstring ext=extension(e);exts.push_back(ext);ns[i].resize(ns[i].size()-ext.size());}}else{for(size_t i=0;i<ns.size();++i)ns[i]+=extensions[i];}textAndExtensionMode(ns,hide,exts);}
    void choose(bool folders,bool children=false){if(!protectDraft())return;Com<IFileOpenDialog>p;if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_ALL,IID_PPV_ARGS(p.out())))){warn(L"无法打开文件选择器。");return;}DWORD opts=0;p->GetOptions(&opts);p->SetOptions(opts|FOS_ALLOWMULTISELECT|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_NOCHANGEDIR|(folders?FOS_PICKFOLDERS:FOS_FILEMUSTEXIST));p->SetTitle(children?L"选择父目录：添加其直接子文件夹":folders?L"按住 Ctrl 或 Shift 多选要重命名的文件夹":L"选择要重命名的文件");if(SUCCEEDED(p->Show(window))){Com<IShellItemArray>result;if(SUCCEEDED(p->GetResults(result.out()))){DWORD n=0;result->GetCount(&n);for(DWORD i=0;i<n;++i){Com<IShellItem>item;LPWSTR path=nullptr;if(SUCCEEDED(result->GetItemAt(i,item.out()))&&SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){if(children){std::wstring error;if(!eAddChildFolders(items,path,error))warn(error);}else addPath(items,path);CoTaskMemFree(path);}}}sortInitial();rebuild();}}
    void sortInitial(){
        std::wstring mode=eSetting(L"Sort",L"Origin");if(mode==L"Origin")return;
        std::stable_sort(items.begin(),items.end(),[mode](const Entry&a,const Entry&b){
            WIN32_FILE_ATTRIBUTE_DATA aa={},bb={};bool metadata=mode!=L"Default"&&mode!=L"FileNameNature"&&mode!=L"FileName";
            if(metadata&&GetFileAttributesExW(nativePath(a.path).c_str(),GetFileExInfoStandard,&aa)&&GetFileAttributesExW(nativePath(b.path).c_str(),GetFileExInfoStandard,&bb)){
                int cmp=0;if(mode.compare(0,8,L"FileSize")==0){ULONGLONG x=((ULONGLONG)aa.nFileSizeHigh<<32)|aa.nFileSizeLow,y=((ULONGLONG)bb.nFileSizeHigh<<32)|bb.nFileSizeLow;cmp=x<y?-1:x>y?1:0;}
                else{const FILETIME*x=mode.compare(0,12,L"CreationTime")==0?&aa.ftCreationTime:mode.compare(0,14,L"LastAccessTime")==0?&aa.ftLastAccessTime:&aa.ftLastWriteTime;const FILETIME*y=mode.compare(0,12,L"CreationTime")==0?&bb.ftCreationTime:mode.compare(0,14,L"LastAccessTime")==0?&bb.ftLastAccessTime:&bb.ftLastWriteTime;cmp=CompareFileTime(x,y);}
                if(cmp)return mode.size()>4&&mode.substr(mode.size()-4)==L"Desc"?cmp>0:cmp<0;
            }
            if(mode==L"FileName")return CompareStringOrdinal(a.name.c_str(),-1,b.name.c_str(),-1,TRUE)==CSTR_LESS_THAN;return StrCmpLogicalW(a.name.c_str(),b.name.c_str())<0;
        });
    }
    bool apply(const ever::Rule&r,bool protectExtension=true){
        std::vector<std::wstring>ns;if(!names(ns))return false;std::vector<std::wstring>out,exts;std::wstring error;
        for(size_t i=0;i<ns.size();++i){std::wstring current=ns[i];if(r.mode==ever::Mode::FindReplace||r.mode==ever::Mode::ExtractBeginning)current=items[i].name;
            std::wstring ext;if(protectExtension&&(!hidden||r.mode==ever::Mode::FindReplace||r.mode==ever::Mode::ExtractBeginning)){Entry e=items[i];e.name=current;ext=extension(e);current.resize(current.size()-ext.size());}
            exts.push_back(ext);ever::Input input;input.path=items[i].path;input.currentName=current;input.originalName=items[i].name;input.ordinal=i;input.totalRows=items.size();
            if(r.mode==ever::Mode::FindReplace)out.push_back(current);else{std::wstring result;if(!ever::apply(r,input,result,error)){warn(items[i].name+L"："+error);return false;}out.push_back(result);}
        }
        if(r.mode==ever::Mode::FindReplace){ever::Input input;input.currentName=eJoin(out);std::wstring replaced;if(!ever::apply(r,input,replaced,error)){warn(error);return false;}out=eLines(replaced);if(out.size()!=items.size()){warn(L"替换结果改变了名称行数，已保留原文本。每行必须仍对应一个项目。");return false;}}
        for(size_t i=0;i<out.size();++i)if(!hidden)out[i]+=exts[i];setText(eJoin(out));return true;
    }
    bool ruleForm(ever::Rule &r,bool &remember){EForm f;std::vector<std::wstring>choices={L"数字递增",L"父目录名",L"创建时间",L"当前时间",L"更新时间",L"图片拍摄时间",L"中文转数字",L"中文转拼音首字母"};const wchar_t*source=r.source==ever::AddSource::Sequence?L"数字递增":r.source==ever::AddSource::ParentName?L"父目录名":r.source==ever::AddSource::CreatedTime?L"创建时间":r.source==ever::AddSource::CurrentTime?L"当前时间":r.source==ever::AddSource::ModifiedTime?L"更新时间":r.source==ever::AddSource::PhotoTime?L"图片拍摄时间":r.source==ever::AddSource::PinyinInitials?L"中文转拼音首字母":r.source==ever::AddSource::ChineseNumbers?L"中文转数字":r.text.c_str();
        f.fields={{101,L"增加还是删减",r.operation==ever::Operation::Delete?L"删减":L"增加",4,{L"增加",L"删减"}},{102,L"开始位置",std::to_wstring(r.index)},{103,L"要增加的字符",source,3,choices},{104,L"删除几个字符",std::to_wstring(r.count)},{105,L"记录日期格式",r.timeFormat},{106,L"前面定位字符",r.frontAnchor},{107,L"后面定位字符",r.backAnchor},{108,L"数字递增从几开始",r.sequenceStart},{109,L"转前几个中文",std::to_wstring(r.pinyinCount)},{110,L"替换整个名称",r.replaceEntireName?L"1":L"0",1},{111,L"连接符",r.separator},{112,L"中文转数字替换选项",r.replaceOriginalNumbers?L"1":L"0",1},{113,L"是否记忆",L"0",1}};
        for(auto&field:f.fields){if(field.id==102)field.help=L"0和1表示开头；-1表示末尾；-2表示倒数第二个字符前。";if(field.id==109)field.help=L"0表示转换全部；正数表示只取名称前N个字符，按原5720条有序表转换后小写。";if(field.id==106||field.id==107)field.help=L"前后定位字符使用.NET正则表达式，留空表示不限定该侧。";if(field.id==111)field.help=L"仅数字递增和父目录名使用连接符，默认下划线。";if(field.id==108)field.help=L"例如01启用原版自动对齐，1不补零；支持小数。";}f.changed=[](EForm &p){std::wstring op=p.get(101),s=p.get(103);for(auto&field:p.fields){int id=field.id;field.visible=id==101||id==102||id==113||(id==104?op==L"删减":id==103||id==110?op==L"增加":id==105?op==L"增加"&&s.size()>=2&&s.substr(s.size()-2)==L"时间":id==106||id==107||id==112?op==L"增加"&&s==L"中文转数字":id==108?op==L"增加"&&s==L"数字递增":id==109?op==L"增加"&&s==L"中文转拼音首字母":id==111?op==L"增加"&&(s==L"数字递增"||s==L"父目录名"):false);}};
        if(!f.run(window,dpi,L"增删转换"))return false;r.mode=ever::Mode::AddDeleteConvert;r.operation=f.value(101)==L"删减"?ever::Operation::Delete:ever::Operation::Add;r.position=ever::Position::Index;if(!eInteger(f.value(102),r.index)||(r.operation==ever::Operation::Delete&&(!eInteger(f.value(104),r.count)||r.count<1))){warn(L"位置和删除数量必须为整数，删除数量至少为1。");return false;}std::wstring s=f.value(103);r.source=s==L"数字递增"?ever::AddSource::Sequence:s==L"父目录名"?ever::AddSource::ParentName:s==L"创建时间"?ever::AddSource::CreatedTime:s==L"当前时间"?ever::AddSource::CurrentTime:s==L"更新时间"?ever::AddSource::ModifiedTime:s==L"图片拍摄时间"?ever::AddSource::PhotoTime:s==L"中文转数字"?ever::AddSource::ChineseNumbers:s==L"中文转拼音首字母"?ever::AddSource::PinyinInitials:ever::AddSource::Text;r.text=s;r.timeFormat=f.value(105);r.frontAnchor=f.value(106);r.backAnchor=f.value(107);r.sequenceStart=f.value(108);if(!eInteger(f.value(109),r.pinyinCount)||r.pinyinCount<0){warn(L"转前几个字符必须为非负整数，0表示全部。");return false;}r.separator=f.value(111);r.replaceEntireName=f.value(110)==L"1";r.replaceOriginalNumbers=f.value(112)==L"1";remember=f.value(113)==L"1";return true;
    }
    bool replacementForm(ever::Rule &r,bool &remember){EForm f;f.fields={{101,L"字符一行一对\n查找内容|替换内容",r.replacements,2},{102,L"转义查找内容",r.escapeFind?L"1":L"0",1},{103,L"转义替换内容",r.escapeReplacement?L"1":L"0",1},{104,L"忽略大小写",r.ignoreCase?L"1":L"0",1},{105,L"使用正则",r.regex?L"1":L"0",1},{106,L"正则单行",r.singleline?L"1":L"0",1},{107,L"正则多行",r.multiline?L"1":L"0",1},{108,L"是否记忆",L"0",1}};for(auto&field:f.fields)if(field.id==101)field.help=L"每行：查找|替换。|||可包含管道符；第一行|=分隔符可指定分隔符。\\n、\\r、\\t由转义选项决定。";f.changed=[](EForm&p){for(auto&field:p.fields)if(field.id==104||field.id==107)field.visible=p.check(105);};if(!f.run(window,dpi,L"改部分 · 仅处理纯文件名"))return false;r.mode=ever::Mode::FindReplace;r.replacements=f.value(101);r.escapeFind=f.value(102)==L"1";r.escapeReplacement=f.value(103)==L"1";r.ignoreCase=f.value(104)==L"1";r.regex=f.value(105)==L"1";r.singleline=f.value(106)==L"1";r.multiline=f.value(107)==L"1";remember=f.value(108)==L"1";return true;}
    void saveTemplate(const ever::Rule&r){std::wstring name;if(!eAsk(window,dpi,L"记忆功能",L"记忆名称",name)||name.empty())return;auto ts=eTemplates();bool found=false;for(auto&t:ts)if(t.name==name){t.rule=r;found=true;break;}if(!found)ts.push_back({name,r});if(!eStoreTemplates(ts))warn(L"记忆模板保存失败。");else status(L"已保存记忆模板："+name);}
    void templates(){auto ts=eTemplates();if(ts.empty())warn(L"还没有记忆模板，请在增删转换或改部分中勾选是否记忆并保存。");HMENU menu=CreatePopupMenu();for(size_t i=0;i<ts.size();++i)AppendMenuW(menu,MF_STRING,EE_TEMPLATEBASE+i,ts[i].name.c_str());AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,EE_BACKUP,L"备份记忆模板…");AppendMenuW(menu,MF_STRING,EE_IMPORT,L"恢复记忆模板…");POINT p;GetCursorPos(&p);UINT selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,window,nullptr);DestroyMenu(menu);if(selected==EE_BACKUP||selected==EE_IMPORT){backup(selected==EE_IMPORT);return;}if(selected<EE_TEMPLATEBASE||selected>=EE_TEMPLATEBASE+ts.size())return;size_t i=selected-EE_TEMPLATEBASE;if(GetKeyState(VK_CONTROL)&0x8000){HMENU edit=CreatePopupMenu();AppendMenuW(edit,MF_STRING,1,L"修改模板");AppendMenuW(edit,MF_STRING,2,L"重命名模板");AppendMenuW(edit,MF_STRING,3,L"删除模板");AppendMenuW(edit,MF_STRING,4,L"复制调用命令");UINT c=TrackPopupMenu(edit,TPM_RETURNCMD,p.x,p.y,0,window,nullptr);DestroyMenu(edit);if(c==1){bool remember=false;bool ok=ts[i].rule.mode==ever::Mode::NamingTemplate?namingForm(ts[i].rule,remember):ts[i].rule.mode==ever::Mode::AddDeleteConvert?ruleForm(ts[i].rule,remember):replacementForm(ts[i].rule,remember);if(ok&&!eStoreTemplates(ts))warn(L"模板保存失败。");}else if(c==2){std::wstring n=ts[i].name;if(eAsk(window,dpi,L"记忆功能",L"新的名称",n)&&!n.empty()){for(size_t j=0;j<ts.size();++j)if(j!=i&&ts[j].name==n){warn(L"模板名称已存在。");return;}ts[i].name=n;if(!eStoreTemplates(ts))warn(L"模板保存失败。");}}else if(c==3){ts.erase(ts.begin()+i);if(!eStoreTemplates(ts))warn(L"模板删除失败。");}else if(c==4){wchar_t exe[32768]={};DWORD hostLength=GetEnvironmentVariableW(L"LITELAUNCH_HOST_EXE",exe,32768);if(!hostLength||hostLength>=32768||GetFileAttributesW(exe)==INVALID_FILE_ATTRIBUTES)GetModuleFileNameW(nullptr,exe,32768);std::wstring n=ts[i].name;if(n.find(L'"')!=n.npos){warn(L"模板名称含引号，请先修改名称后复制命令。");return;}eCopy(window,L"\""+std::wstring(exe)+L"\" --rename-action \"记忆功能："+n+L"\"");}return;}if(apply(ts[i].rule)&&execute())DestroyWindow(window);}
    void backup(bool restore){std::wstring path;if(!ePickPath(window,!restore,path))return;if(restore){auto bytes=eRead(path);if(bytes.empty()){warn(L"模板备份无效或不可访问。");return;}auto old=eRead(eHome()+L"templates.bin");if(!eAtomic(eHome()+L"templates.bin",bytes)){warn(L"模板恢复失败。");return;}try{eTemplates();}catch(...){if(!old.empty())eAtomic(eHome()+L"templates.bin",old);else DeleteFileW((eHome()+L"templates.bin").c_str());warn(L"备份格式无效，原模板已保留。");return;}status(L"已恢复记忆模板。");}else{auto bytes=eRead(eHome()+L"templates.bin");if(bytes.empty()){warn(L"当前没有可备份的模板。");return;}if(!eAtomic(path,bytes))warn(L"备份写入失败。");else status(L"记忆模板已备份，可保存到网盘同步目录。");}}
    void settings(){
        EForm f;f.fields={{101,L"背景颜色（RRGGBB）",eSetting(L"Background",L"FFFFFF")},{102,L"文字颜色（RRGGBB）",eSetting(L"Foreground",L"202020")},{103,L"字体大小",eSetting(L"FontSize",eDefaultFontSize())},{104,L"字体名称",eSetting(L"Font",L"Microsoft YaHei UI")},{105,L"默认排序",eSortLabel(),4,eSortLabels()},{106,L"剪贴板等待毫秒",eSetting(L"ClipboardWait",L"1000")},{107,L"模板备份、恢复",L"不操作",4,{L"不操作",L"备份到文件",L"从文件恢复"}},{108,L"默认隐藏扩展名",eSetting(L"HideExtension",L"0"),1},{109,L"重命名后刷新",eSetting(L"Refresh",L"0"),1}};
        f.extraLabel=L"选择颜色 / 字体";f.extra=[](EForm&p){HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"背景颜色…");AppendMenuW(menu,MF_STRING,2,L"文字颜色…");AppendMenuW(menu,MF_STRING,3,L"字体…");POINT point;GetCursorPos(&point);UINT id=TrackPopupMenu(menu,TPM_RETURNCMD,point.x,point.y,0,p.window,nullptr);DestroyMenu(menu);if(id==1||id==2){std::wstring value=p.get(100+id);if(!value.empty()&&value[0]==L'#')value.erase(0,1);DWORD rgb=wcstoul(value.c_str(),nullptr,16);static COLORREF custom[16]={};CHOOSECOLORW cc={sizeof(cc)};cc.hwndOwner=p.window;cc.rgbResult=RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255);cc.lpCustColors=custom;cc.Flags=CC_FULLOPEN|CC_RGBINIT;if(ChooseColorW(&cc)){wchar_t text[10];swprintf(text,10,L"%02X%02X%02X",GetRValue(cc.rgbResult),GetGValue(cc.rgbResult),GetBValue(cc.rgbResult));p.set(100+id,text);}}else if(id==3){LOGFONTW font={};font.lfHeight=-p.px(std::max(6,p.num(103)));std::wstring name=p.get(104);wcsncpy(font.lfFaceName,name.c_str(),LF_FACESIZE-1);CHOOSEFONTW choose={sizeof(choose)};choose.hwndOwner=p.window;choose.lpLogFont=&font;choose.Flags=CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT;if(ChooseFontW(&choose)){p.set(104,font.lfFaceName);p.set(103,std::to_wstring(std::max(6,choose.iPointSize/10)));}}};
        if(!f.run(window,dpi,L"EVER重命名 · 设置"))return;int size=0,wait=0;if(!eInteger(f.value(103),size)||!eInteger(f.value(106),wait)||size<6||size>72||wait<200||wait>9000){warn(L"字号应为6—72，剪贴板等待应为200—9000毫秒。");return;}if(f.value(104).empty())for(auto&field:f.fields)if(field.id==104)field.value=L"Microsoft YaHei UI";
        for(auto&field:f.fields)if(field.id==101||field.id==102){std::wstring s=field.value;if(s.empty())s=field.id==101?L"FFFFFF":L"202020";if(s[0]==L'#')s.erase(0,1);field.value=s;if(s.size()!=6||s.find_first_not_of(L"0123456789abcdefABCDEF")!=s.npos){warn(L"颜色请填写6位十六进制值，如FFFFFF。");return;}}
        const wchar_t*keys[]={L"Background",L"Foreground",L"FontSize",L"Font"};for(int i=0;i<4;++i)eSaveSetting(keys[i],f.value(101+i));auto labels=eSortLabels();for(int i=0;i<12;++i)if(f.value(105)==labels[i])eSaveSetting(L"Sort",eSortValues[i]);eSaveSetting(L"ClipboardWait",f.value(106));eSaveSetting(L"HideExtension",f.value(108));eSaveSetting(L"Refresh",f.value(109));appearance();if(f.value(107)!=L"不操作")backup(f.value(107)==L"从文件恢复");
    }
    void appearance(){std::string name=eUtf8(eSetting(L"Font",L"Microsoft YaHei UI"));sci(SCI_STYLESETFONT,STYLE_DEFAULT,(LPARAM)name.c_str());sci(SCI_STYLESETSIZE,STYLE_DEFAULT,_wtoi(eSetting(L"FontSize",eDefaultFontSize()).c_str()));auto color=[](std::wstring s){DWORD n=wcstoul(s.c_str(),nullptr,16);return RGB((n>>16)&255,(n>>8)&255,n&255);};sci(SCI_STYLESETBACK,STYLE_DEFAULT,color(eSetting(L"Background",L"FFFFFF")));sci(SCI_STYLESETFORE,STYLE_DEFAULT,color(eSetting(L"Foreground",L"202020")));sci(SCI_STYLECLEARALL);sci(SCI_SETWRAPMODE,SC_WRAP_WORD);
#ifdef LITELAUNCH_BATCH_PLUGIN
sci(SCI_STYLESETFORE,STYLE_LINENUMBER,UI_FAINT);sci(SCI_STYLESETBACK,STYLE_LINENUMBER,UI_SURFACE_ALT);sci(SCI_STYLESETSIZE,STYLE_LINENUMBER,10);sci(SCI_SETMARGINLEFT,0,px(10));sci(SCI_SETMARGINWIDTHN,0,px(38));
#endif
}
    bool namingForm(ever::Rule&r,bool&remember,bool allowMemory=true){
        EForm f;f.fields={{101,L"名称格式（自动保留扩展名）",r.namingPattern,3,{L"{name}-{num}",L"xxxx-{num}",L"{taken:yyyyMMdd_HHmmss}-{num}",L"{created:yyyyMMdd}-{name}-{num}"}},
            {102,L"编号起始数字",r.sequenceStart},{103,L"编号位数（1—18）",std::to_wstring(r.sequenceWidth)},{104,L"编号步长",std::to_wstring(r.sequenceStep)},
            {105,L"默认日期格式",r.timeFormat},{106,L"是否记忆为模板",L"0",1}};
        f.fields.back().visible=allowMemory;
        f.fields[0].help=L"{name}当前名称，{num}编号，{taken}EXIF拍摄时间，{created}创建时间，{modified}修改时间，{parent}父目录，{now}当前时间。日期可写{taken:yyyyMMdd_HHmmss}。无拍摄时间会提示并保持全部原文。";
        f.extraLabel=L"插入字段";f.extra=[](EForm&p){HMENU m=CreatePopupMenu();const wchar_t*labels[]={L"当前名称",L"编号",L"EXIF拍摄时间",L"创建时间",L"修改时间",L"父目录名",L"当前时间"};const wchar_t*tokens[]={L"{name}",L"{num}",L"{taken}",L"{created}",L"{modified}",L"{parent}",L"{now}"};for(int i=0;i<7;i++)AppendMenuW(m,MF_STRING,i+1,labels[i]);POINT at;GetCursorPos(&at);int choice=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,at.x,at.y,0,p.window,nullptr);DestroyMenu(m);if(choice>=1&&choice<=7)p.set(101,p.get(101)+tokens[choice-1]);};
        if(!f.run(window,dpi,L"编号与元数据 · 确定后预览"))return false;
        int width=0,step=0;if(!eInteger(f.value(103),width)||width<1||width>18||!eInteger(f.value(104),step)||step<1){warn(L"编号位数须为1—18，步长须为正整数。");return false;}
        r.mode=ever::Mode::NamingTemplate;r.namingPattern=f.value(101);r.sequenceStart=f.value(102);r.sequenceWidth=width;r.sequenceStep=step;r.timeFormat=f.value(105);remember=f.value(106)==L"1";return true;
    }
    void naming(){
        ever::Rule r;r.mode=ever::Mode::NamingTemplate;r.namingPattern=eSetting(L"NamingPattern",L"{name}-{num}");r.sequenceStart=eSetting(L"NamingStart",L"1");r.sequenceWidth=_wtoi(eSetting(L"NamingWidth",L"3").c_str());r.sequenceStep=_wtoi(eSetting(L"NamingStep",L"1").c_str());r.timeFormat=eSetting(L"NamingDateFormat",L"yyyyMMdd_HHmmss");
        bool remember=false;if(!namingForm(r,remember))return;
        if(apply(r)){eSaveSetting(L"NamingPattern",r.namingPattern);eSaveSetting(L"NamingStart",r.sequenceStart);eSaveSetting(L"NamingWidth",std::to_wstring(r.sequenceWidth));eSaveSetting(L"NamingStep",std::to_wstring(r.sequenceStep));eSaveSetting(L"NamingDateFormat",r.timeFormat);if(remember)saveTemplate(r);}
    }
    void findReplace(){
        EForm f;std::wstring selection=selectedText();f.fields={{101,L"查找",selection},{102,L"替换为",L""},{103,L"正则表达式",L"0",1},{104,L"忽略大小写",L"0",1},{105,L"只替换选中内容",L"0",1},{106,L"单行（.匹配换行）",L"0",1},{107,L"多行（^/$匹配行）",L"1",1}};
        sptr_t originalStart=sci(SCI_GETSELECTIONSTART),originalEnd=sci(SCI_GETSELECTIONEND);int next=0;f.extraLabel=L"查找下一个";
        f.extra=[this,&next](EForm&p){std::wstring pattern=p.get(101),input=text();if(pattern.empty()){MessageBoxW(p.window,L"请填写查找内容。",L"查找",MB_ICONINFORMATION);return;}
            if(!p.check(103)){std::wstring literal;for(wchar_t c:pattern){if(wcschr(L"\\.^$|?*+()[]{}",c))literal+=L'\\';literal+=c;}pattern=literal;}
            int found=-1,length=0;std::wstring err;if(next>(int)input.size())next=0;
            if(!ever::regexFind(pattern,input,next,p.check(104),p.check(106),p.check(107),found,length,err)){MessageBoxW(p.window,err.c_str(),L"查找",MB_ICONWARNING);return;}
            if(found<0&&next>0){next=0;if(!ever::regexFind(pattern,input,0,p.check(104),p.check(106),p.check(107),found,length,err)){MessageBoxW(p.window,err.c_str(),L"查找",MB_ICONWARNING);return;}}
            if(found<0){MessageBoxW(p.window,L"没有找到匹配内容。",L"查找",MB_ICONINFORMATION);return;}size_t a=eUtf8(input.substr(0,found)).size(),b=eUtf8(input.substr(0,found+length)).size();sci(SCI_SETSEL,a,b);sci(SCI_SCROLLCARET);next=found+std::max(length,1);
        };
        if(!f.run(window,dpi,L"查找替换（默认全部名称，确定后预览）"))return;if(f.value(101).empty()){warn(L"查找内容不能为空。");return;}std::wstring input=text();bool selected=f.value(105)==L"1";sptr_t start=0,end=sci(SCI_GETTEXTLENGTH);
        if(selected){start=originalStart;end=originalEnd;if(start==end){warn(L"请先选中要查找替换的内容。");return;}std::string bytes=eUtf8(input);input=eWide(bytes.substr(start,end-start));}
        std::wstring out,error;if(!ever::regexReplace(f.value(101),f.value(102),input,f.value(103)==L"1",f.value(104)==L"1",f.value(106)==L"1",f.value(107)==L"1",false,false,out,error)){warn(error);return;}
        std::string bytes=eUtf8(out),complete=eUtf8(text());complete.replace((size_t)start,(size_t)(end-start),bytes);
        if(eLines(eWide(complete)).size()!=eLines(text()).size()){warn(L"替换结果改变了名称行数，已保留原文本。每行必须仍对应一个项目。");return;}
        sci(SCI_BEGINUNDOACTION);sci(SCI_SETTARGETSTART,start);sci(SCI_SETTARGETEND,end);sci(SCI_REPLACETARGET,bytes.size(),(LPARAM)bytes.data());sci(SCI_ENDUNDOACTION);update();
    }
    void regret(){if(history.empty()){warn(L"没有可撤回的重命名记录。");return;}std::vector<Move>reverse;for(const auto&m:history){Move r=m;r.before=m.after;r.after=m.before;reverse.push_back(r);}Result result=transact(reverse);if(result.ok){history=eRegretSuccess(history,result.undo);for(auto&e:items)for(const auto&m:result.undo)if(same(e.path,m.before)){e.path=m.after;e.name=fileName(m.after);break;}if(!eStoreHistory(history))warn(L"名称已恢复，但后悔药记录写入失败，请保留本窗口。");rebuild();status(history.empty()?L"失败事务已恢复完成。":L"后悔药已执行；再次执行可恢复改后的名称。");}else{if(!result.undo.empty()){reconcileUndo(history,result.undo);for(auto&e:items)for(const auto&m:result.undo)if(same(e.path,m.before)){e.path=m.after;e.name=fileName(m.after);break;}rebuild();}if(!eStoreHistory(history))warn(L"恢复记录写入失败，请保留本窗口。");warn(result.error);}}
    bool execute(){
        std::vector<std::wstring>ns;if(!names(ns)||items.empty())return false;
#ifdef LITELAUNCH_BATCH_PLUGIN
for(size_t i=0;i<ns.size();i++)if(ns[i].empty()||(!hidden&&i<extensions.size()&&!extensions[i].empty()&&ns[i]==extensions[i])){warn(L"名称已清空，请先添加文字或编号再执行。空行会保留项目对应关系。");return false;}
#endif
for(size_t i=0;i<ns.size();++i)if(hidden)ns[i]+=extensions[i];
        std::wstring error;bool resolved=false;if(!eResolveNames(items,ns,error,resolved)){warn(error);return false;}
        if(resolved){std::vector<std::wstring>shown=ns;if(hidden)for(size_t i=0;i<shown.size();++i){Entry e=items[i];e.name=shown[i];std::wstring ext=extension(e);extensions[i]=ext;shown[i].resize(shown[i].size()-ext.size());}setText(eJoin(shown));status(L"同名项目已添加数字后缀，请检查名称后再次点击确定。");return false;}
        for(size_t i=0;i<items.size();++i)items[i].planned=ns[i];std::vector<Move>moves;if(!makeMoves(items,moves,error)){warn(error);return false;}
        Result result=transact(moves);if(result.ok){history=result.undo;for(auto&e:items)for(const auto&m:result.undo)if(same(e.path,m.before)){e.path=m.after;e.name=fileName(m.after);break;}bool saved=eStoreHistory(history);if(!saved)warn(L"重命名已完成，但后悔药记录写入失败。请保留本窗口。");rebuild();status(L"重命名完成，后悔药可跨窗口使用。");if(eSetting(L"Refresh",L"0")==L"1")refreshSource();return saved;}
        if(!result.undo.empty()){history=result.undo;if(!eStoreHistory(history))warn(L"恢复记录写入失败，请保留本窗口。");for(auto&e:items)for(const auto&m:result.undo)if(same(e.path,m.before)){e.path=m.after;e.name=fileName(m.after);break;}rebuild();}warn(result.error);return false;
    }
    void directory(){warn(L"原动作的取目录功能还未实现。");}
    std::vector<std::pair<std::wstring,std::wstring>> linkApps(){std::vector<std::pair<std::wstring,std::wstring>> apps;auto lines=eLines(eSetting(L"LinkApps",L"Excel|C:\\Program Files\\Microsoft Office\\root\\Office16\\EXCEL.EXE\nVim|E:\\MyTools\\Vim\\vim81\\gvim.exe"));if(lines.size()==1&&lines[0].find(L'\t')!=lines[0].npos){auto value=lines[0];std::replace(value.begin(),value.end(),L'\t',L'\n');lines=eLines(value);}for(const auto&line:lines){size_t at=line.find(L'|');if(at!=line.npos&&at>0&&at+1<line.size())apps.push_back({line.substr(0,at),line.substr(at+1)});}return apps;}
    void linked(bool configure=false,int index=0){
        if(configure){std::wstring list;for(const auto&app:linkApps()){if(!list.empty())list+=L'\n';list+=app.first+L"|"+app.second;}if(eAsk(window,dpi,L"设置联动修改",L"每行：名称|程序完整路径",list,true)){std::wstring encoded;for(wchar_t ch:list){if(ch==L'\n')encoded+=L'\t';else if(ch!=L'\r')encoded+=ch;}eSaveSetting(L"LinkApps",encoded);}return;}
        auto apps=linkApps();if(index<0||index>=(int)apps.size())return;std::wstring path=apps[index].second;eCopy(window,text());
        if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES){warn(L"联动程序不存在，请在设置联动修改中修改路径。名称已复制，可在其他编辑器修改后粘贴回来。");return;}
        SHELLEXECUTEINFOW launch={sizeof(launch)};launch.fMask=SEE_MASK_NOCLOSEPROCESS;launch.hwnd=window;launch.lpVerb=L"open";launch.lpFile=path.c_str();launch.nShow=SW_SHOWNORMAL;
        if(!ShellExecuteExW(&launch)){warn(L"联动程序启动失败。");return;}if(launch.hProcess){WaitForInputIdle(launch.hProcess,3000);CloseHandle(launch.hProcess);}
        HWND foreground=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(foreground,&pid);wchar_t executable[32768]={};HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);DWORD length=32768;
        bool correct=process&&QueryFullProcessImageNameW(process,0,executable,&length)&&same(fullPath(executable),fullPath(path));if(process)CloseHandle(process);
        if(correct){INPUT keys[4]={};for(auto&key:keys)key.type=INPUT_KEYBOARD;keys[0].ki.wVk=VK_CONTROL;keys[1].ki.wVk='V';keys[2].ki.wVk='V';keys[2].ki.dwFlags=KEYEVENTF_KEYUP;keys[3].ki.wVk=VK_CONTROL;keys[3].ki.dwFlags=KEYEVENTF_KEYUP;SendInput(4,keys,sizeof(INPUT));}
        status(L"在联动程序中编辑，复制结果并粘贴回此窗口。");
    }
    void speak(){CLSID cls;if(!voice.p&&(FAILED(CLSIDFromProgID(L"SAPI.SpVoice",&cls))||FAILED(CoCreateInstance(cls,nullptr,CLSCTX_ALL,IID_PPV_ARGS(voice.out()))))){warn(L"Windows朗读服务不可用。");return;}LPOLESTR method=(LPOLESTR)L"Speak";DISPID id;if(FAILED(voice->GetIDsOfNames(IID_NULL,&method,1,LOCALE_USER_DEFAULT,&id)))return;VARIANT args[2];VariantInit(&args[0]);VariantInit(&args[1]);args[0].vt=VT_I4;args[0].lVal=3;args[1].vt=VT_BSTR;std::wstring input=selectedText();if(input.empty())input=text();args[1].bstrVal=SysAllocString(input.c_str());DISPPARAMS parameters={args,nullptr,2,0};voice->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_METHOD,&parameters,nullptr,nullptr,nullptr);VariantClear(&args[1]);}
    void highlight(){std::string selected=eUtf8(selectedText()),all=eUtf8(text());sci(SCI_SETINDICATORCURRENT,8);sci(SCI_INDICATORCLEARRANGE,0,all.size());if(selected.empty())return;sci(SCI_INDICSETSTYLE,8,INDIC_ROUNDBOX);sci(SCI_INDICSETFORE,8,RGB(255,168,0));sci(SCI_INDICSETALPHA,8,90);size_t at=0;while((at=all.find(selected,at))!=all.npos){sci(SCI_INDICATORFILLRANGE,at,selected.size());at+=selected.size();}}
    void menu(int which){
        HMENU m=CreatePopupMenu();if(which==EE_EXT){AppendMenuW(m,MF_STRING,EE_HIDE,L"隐藏扩展(&1)");AppendMenuW(m,MF_STRING,EE_SHOW,L"显示扩展(&2)");}
        else if(which==EE_EDITOR){const int ids[]={EE_CUT,EE_COPY,EE_PASTE,EE_SELECTALL,EE_UNDO,EE_REDO,EE_RESTORE,EE_PIN,EE_HIGHLIGHT};const wchar_t*labels[]={L"剪切",L"复制",L"粘贴",L"全选",L"撤销",L"重做",L"恢复初始名称",L"置顶",L"高亮选中内容"};for(int i=0;i<9;++i)AppendMenuW(m,MF_STRING,ids[i],labels[i]);}
        else if(which==EE_LINK){auto apps=linkApps();for(size_t i=0;i<apps.size();++i)AppendMenuW(m,MF_STRING,EE_LINKBASE+i,apps[i].first.c_str());AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,EE_LINKSET,L"设置联动修改…");}
        else{AppendMenuW(m,MF_STRING,EE_NAMING,L"编号与元数据…");const wchar_t*labels[]={L"数字排(&1)",L"改扩展(&2)",L"取文首(&3)",L"改部分(&4)",L"取目录(&5)",L"计字符(&6)"};for(int i=0;i<6;++i)AppendMenuW(m,MF_STRING,EE_NUMBER+i,labels[i]);AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,EE_REGRET,L"后悔药");AppendMenuW(m,MF_STRING,EE_TEMPLATES,L"记忆功能（Ctrl点击修改）");AppendMenuW(m,MF_STRING,EE_SETTINGS,L"设置…");}
        POINT p;GetCursorPos(&p);UINT id=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,window,nullptr);DestroyMenu(m);if(id)command(id);
    }
    void refreshSource(){if(!source||!IsWindow(source))return;HWND target=GetAncestor(source,GA_ROOT);wchar_t cls[100]={};GetClassNameW(target,cls,100);if(wcscmp(cls,L"CabinetWClass")&&wcscmp(cls,L"TTOTAL_CMD")&&wcscmp(cls,L"Progman")&&wcscmp(cls,L"WorkerW")&&wcscmp(cls,L"EVERYTHING"))return;Sleep(2000);SetForegroundWindow(target);if(GetForegroundWindow()!=target)return;bool tc=!wcscmp(cls,L"TTOTAL_CMD");INPUT keys[4]={};int n=tc?4:2;for(auto&key:keys)key.type=INPUT_KEYBOARD;if(tc){keys[0].ki.wVk=VK_CONTROL;keys[1].ki.wVk='R';keys[2].ki.wVk='R';keys[2].ki.dwFlags=KEYEVENTF_KEYUP;keys[3].ki.wVk=VK_CONTROL;keys[3].ki.dwFlags=KEYEVENTF_KEYUP;}else{keys[0].ki.wVk=VK_F5;keys[1].ki.wVk=VK_F5;keys[1].ki.dwFlags=KEYEVENTF_KEYUP;}SendInput(n,keys,sizeof(INPUT));}
    void command(int id){
#ifdef LITELAUNCH_BATCH_PLUGIN
        if(id==EE_OTHER){batchMore();return;}
        if(id>=BR_ADD&&id<=BR_QREDO){batchCommand(id);return;}
#endif

        if(id>=EE_LINKBASE&&id<EE_LINKBASE+10000){linked(false,id-EE_LINKBASE);return;}
        if(id==EE_OK){if(execute())DestroyWindow(window);}
        else if(id==EE_FIND||id==EE_SEARCH)findReplace();
        else if(id==EE_RULE){ever::Rule r;r.position=ever::Position::Index;r.index=0;r.separator=eSetting(L"Separator",L"_");r.sequenceStart=L"01";r.timeFormat=eSetting(L"DateFormat",L"yyyy-MM-dd HH-mm-ss");r.text=eSetting(L"LastSource",L"");bool remember=false;if(ruleForm(r,remember)){eSaveSetting(L"DateFormat",r.timeFormat);eSaveSetting(L"Separator",r.separator);eSaveSetting(L"LastSource",r.text);if(remember)saveTemplate(r);else apply(r);}}
        else if(id==EE_NAMING)naming();
        else if(id==EE_PART){ever::Rule r;r.mode=ever::Mode::FindReplace;bool remember=false;if(replacementForm(r,remember)){if(remember)saveTemplate(r);else apply(r);}}
        else if(id==EE_OTHER||id==EE_EXT||id==EE_LINK||id==EE_EDITOR)menu(id);
        else if(id==EE_HIDE||id==EE_SHOW)toggle(id==EE_HIDE);
        else if(id==EE_ADD||id==EE_FOLDER){
#ifdef LITELAUNCH_BATCH_PLUGIN
if(id==EE_FOLDER)batchFolderMenu();else choose(false);
#else
choose(id==EE_FOLDER);
#endif
}
        else if(id==EE_READ){if(!protectDraft())return;std::vector<Entry>selected;if(!eSourceSelection(source,selected))warn(L"没有读取到原窗口中的明确选中项。可添加或拖入项目。");else{items=selected;sortInitial();rebuild();}SetForegroundWindow(window);}
        else if(id==EE_ZOOMIN||id==EE_ZOOMOUT)sci(SCI_ZOOMIN+(id==EE_ZOOMOUT));
        else if(id==EE_UNDO||id==EE_REDO){sci(id==EE_UNDO?SCI_UNDO:SCI_REDO);update();}
        else if(id==EE_COPY){if(sci(SCI_GETSELECTIONEMPTY))eCopy(window,text());else sci(SCI_COPY);}
        else if(id==EE_CUT)sci(SCI_CUT);else if(id==EE_PASTE)sci(SCI_PASTE);else if(id==EE_SELECTALL)sci(SCI_SELECTALL);
        else if(id==EE_RESTORE)restoreNames();else if(id==EE_PIN)SetWindowPos(window,(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOPMOST)?HWND_NOTOPMOST:HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
        else if(id==EE_HIGHLIGHT)highlight();else if(id==EE_SPEAK)speak();else if(id==EE_REGRET)regret();else if(id==EE_TEMPLATES)templates();else if(id==EE_SETTINGS)settings();else if(id==EE_LINKSET)linked(true);else if(id==EE_DIRECTORY)directory();
        else if(id==EE_COUNT)warn(L"选中字符数："+std::to_wstring(selectedText().size()));
        else if(id==EE_NUMBER){ever::Rule r;r.mode=ever::Mode::NumberArrange;r.sequenceStart=L"01";if(eAsk(window,dpi,L"数字排",L"从几开始（前置0对齐）",r.sequenceStart))apply(r);}
        else if(id==EE_BEGINNING){std::wstring count=L"12";if(eAsk(window,dpi,L"取文首",L"取前几个合法非空字符",count)){ever::Rule r;r.mode=ever::Mode::ExtractBeginning;if(!eInteger(count,r.extractLength)||r.extractLength<0){warn(L"请输入非负整数。");return;}apply(r);}}
        else if(id==EE_CHANGEEXT){if(hidden){warn(L"扩展名已隐藏，请先显示扩展名再使用改扩展。");return;}std::wstring ext=L".";if(eAsk(window,dpi,L"改扩展",L"新扩展名（带.）",ext)){std::vector<std::wstring>ns;if(!names(ns))return;for(size_t i=0;i<ns.size();++i)if(!items[i].folder){Entry e=items[i];e.name=ns[i];auto old=extension(e);ns[i].resize(ns[i].size()-old.size());ns[i]+=ext;}setText(eJoin(ns));}}
    }
#ifdef LITELAUNCH_BATCH_PLUGIN
#include "batch_window.inc"
#endif
    void controls(){font=CreateFontW(-px(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");titleFont=CreateFontW(-px(15),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");background=CreateSolidBrush(UI_CANVAS);const int ids[]={EE_ZOOMIN,EE_ZOOMOUT,EE_COPY,EE_SPEAK,EE_SEARCH,EE_UNDO,EE_REDO,EE_OK,EE_FIND,EE_RULE,EE_NAMING,EE_OTHER,EE_EXT,EE_LINK,EE_ADD,EE_FOLDER,EE_READ};const wchar_t*labels[]={L"放大",L"缩小",L"复制",L"朗读",L"查找",L"撤销",L"重做",L"确定(&S)",L"查找替换(&F)",L"增删转换(&Q)",L"编号模板(&N)",L"其他操作(&E)",L"显隐扩展(&X)",L"联动修改(&D)",L"添加文件",L"添加文件夹",L"读取选中项"};for(int i=0;i<17;++i){HWND h=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,window,(HMENU)(INT_PTR)ids[i],GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);eTrackHover(h,&hot);}editor=CreateWindowExW(WS_EX_CLIENTEDGE,L"Scintilla",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|WS_HSCROLL,0,0,0,0,window,(HMENU)(INT_PTR)EE_EDITOR,GetModuleHandleW(nullptr),nullptr);if(!editor)throw std::runtime_error("Editor creation failed");LiteLaunchEditor::Configure(editor,14);sci(SCI_USEPOPUP,SC_POPUP_NEVER);appearance();HWND statusWindow=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,0,0,window,(HMENU)(INT_PTR)EE_STATUS,GetModuleHandleW(nullptr),nullptr);SendMessageW(statusWindow,WM_SETFONT,(WPARAM)font,TRUE);DragAcceptFiles(window,TRUE);
#ifdef LITELAUNCH_BATCH_PLUGIN
        batchControls();
#endif
    }
    void layout(){RECT r;GetClientRect(window,&r);int width=r.right,height=r.bottom;const int top[]={EE_ZOOMIN,EE_ZOOMOUT,EE_COPY,EE_SPEAK,EE_SEARCH,EE_UNDO,EE_REDO};for(int i=0;i<7;++i)MoveWindow(ctl(top[i]),px(EVER_MARGIN)+i*px(68),px(60),px(62),px(30),TRUE);const int bar[]={EE_OK,EE_FIND,EE_RULE,EE_NAMING,EE_OTHER,EE_EXT,EE_LINK};int button=std::max(px(90),(width-px(32))/7);for(int i=0;i<7;++i)MoveWindow(ctl(bar[i]),px(EVER_MARGIN)+i*button,px(98),button-px(6),px(34),TRUE);MoveWindow(editor,px(EVER_MARGIN),px(146),width-px(32),std::max(px(80),height-px(214)),TRUE);for(int i=0;i<3;++i)MoveWindow(ctl(EE_ADD+i),px(EVER_MARGIN)+i*px(120),height-px(52),px(112),px(32),TRUE);MoveWindow(ctl(EE_STATUS),px(384),height-px(46),std::max(px(60),width-px(400)),px(22),TRUE);
#ifdef LITELAUNCH_BATCH_PLUGIN
        batchLayout(width,height);
#endif
    }
    /* Slim header band: UI_SURFACE, an accent pill and the window title, closed
       by a 1px UI_LINE hairline.  Everything else is the UI_CANVAS client area. */
    void paint(HDC dc,const RECT &client){
#ifdef LITELAUNCH_BATCH_PLUGIN
        batchPaint(dc,client);return;
#endif
        HBRUSH canvas=CreateSolidBrush(UI_CANVAS);FillRect(dc,&client,canvas);DeleteObject(canvas);
        RECT header={client.left,client.top,client.right,client.top+px(EVER_HEADER)};
        HBRUSH surface=CreateSolidBrush(UI_SURFACE);FillRect(dc,&header,surface);DeleteObject(surface);
        COLORREF accent=glassAccentColor();
        RECT pill={px(EVER_MARGIN),px(13),px(EVER_MARGIN)+px(4),px(35)};
        eRoundRect(dc,pill,px(2),accent,accent);
        HPEN line=CreatePen(PS_SOLID,1,UI_LINE);HGDIOBJ old=SelectObject(dc,line);
        MoveToEx(dc,client.left,px(EVER_HEADER-1),nullptr);LineTo(dc,client.right,px(EVER_HEADER-1));
        SelectObject(dc,old);DeleteObject(line);
        wchar_t title[512]=L"";GetWindowTextW(window,title,512);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,UI_INK);SelectObject(dc,titleFont?titleFont:font);
        RECT text={px(EVER_MARGIN+16),client.top,client.right-px(EVER_MARGIN),client.top+px(EVER_HEADER)};
        DrawTextW(dc,title,-1,&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    }
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){EverWindow*d=(EverWindow*)GetWindowLongPtrW(w,GWLP_USERDATA);if(m==WM_NCCREATE){d=(EverWindow*)((CREATESTRUCTW*)b)->lpCreateParams;d->window=w;eCurrentWindow=w;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)d);}if(!d)return DefWindowProcW(w,m,a,b);try{switch(m){case WM_CREATE:d->controls();d->layout();d->rebuild();return 0;case WM_ERASEBKGND:return 1;case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT client;GetClientRect(w,&client);d->paint(dc,client);EndPaint(w,&ps);return 0;}case WM_DRAWITEM:{DRAWITEMSTRUCT *item=(DRAWITEMSTRUCT*)b;if(item->CtlType!=ODT_BUTTON)return FALSE;eDrawOwnerButton(item,d->dpi,
#ifdef LITELAUNCH_BATCH_PLUGIN
item->CtlID==EE_OK?UI_CANVAS:UI_SURFACE,
#else
UI_CANVAS,
#endif
item->CtlID==EE_OK
#ifdef LITELAUNCH_BATCH_PLUGIN
||item->CtlID==BR_ADD
#endif
,(item->itemState&ODS_HOTLIGHT)!=0||d->hot==(int)item->CtlID);return TRUE;}case WM_CTLCOLORSTATIC:{HDC dc=(HDC)a;SetBkMode(dc,TRANSPARENT);SetTextColor(dc,UI_MUTED);return (LRESULT)(d->background?d->background:(HBRUSH)GetStockObject(WHITE_BRUSH));}case WM_SETTEXT:{LRESULT result=DefWindowProcW(w,m,a,b);RECT head;GetClientRect(w,&head);head.bottom=d->px(EVER_HEADER);InvalidateRect(w,&head,FALSE);return result;}case WM_SIZE:if(d->editor)d->layout();return 0;case WM_GETMINMAXINFO:{
#ifdef LITELAUNCH_BATCH_PLUGIN
        MONITORINFO monitor={sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST),&monitor);((MINMAXINFO*)b)->ptMinTrackSize={std::min(d->px(920),(int)(monitor.rcWork.right-monitor.rcWork.left)),std::min(d->px(620),(int)(monitor.rcWork.bottom-monitor.rcWork.top))};
#else
        ((MINMAXINFO*)b)->ptMinTrackSize={d->px(700),d->px(420)};
#endif
        return 0;}case WM_COMMAND:if(LOWORD(a)!=EE_EDITOR&&HIWORD(a)==0)d->command(LOWORD(a));return 0;case WM_TIMER:
#ifdef LITELAUNCH_BATCH_PLUGIN
        if(a==8201){KillTimer(w,8201);d->batchFiles();return 0;}
#endif
        break;case WM_NOTIFY:
#ifdef LITELAUNCH_BATCH_PLUGIN
        if(((NMHDR*)b)->code==NM_CUSTOMDRAW&&(((NMHDR*)b)->idFrom==BR_RULES||((NMHDR*)b)->idFrom==BR_FILES))return d->batchCustomDraw((NMLVCUSTOMDRAW*)b);
        d->batchNotify((NMHDR*)b);
#endif
        if(((NMHDR*)b)->idFrom==EE_EDITOR&&((NMHDR*)b)->code==SCN_MODIFIED){d->extensionNotification((SCNotification*)b);d->update();}return 0;case WM_CONTEXTMENU:d->menu((HWND)a==d->editor?EE_EDITOR:EE_OTHER);return 0;case WM_DROPFILES:{HDROP drop=(HDROP)a;if(!d->protectDraft()){DragFinish(drop);return 0;}UINT n=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);for(UINT i=0;i<n;++i){UINT len=DragQueryFileW(drop,i,nullptr,0);std::wstring p(len+1,0);DragQueryFileW(drop,i,&p[0],len+1);p.resize(len);addPath(d->items,p);}DragFinish(drop);d->sortInitial();d->rebuild();return 0;}case WM_CLOSE:DestroyWindow(w);return 0;case WM_NCDESTROY:
#ifdef LITELAUNCH_BATCH_PLUGIN
        if(d->batchTitleFont)DeleteObject(d->batchTitleFont);d->batchTitleFont=nullptr;if(d->batchRowImages)ImageList_Destroy(d->batchRowImages);d->batchRowImages=nullptr;
#endif
        if(d->font)DeleteObject(d->font);d->font=nullptr;if(d->titleFont)DeleteObject(d->titleFont);d->titleFont=nullptr;if(d->background)DeleteObject(d->background);d->background=nullptr;if(eCurrentWindow==w)eCurrentWindow=nullptr;d->window=nullptr;break;}}catch(const std::exception&){d->warn(L"操作遇到错误，已停止。请保留本窗口检查文件和可用内存。");return m==WM_CREATE?-1:0;}return DefWindowProcW(w,m,a,b);}
};
static void everShow(HWND owner,HWND source,int dpi,const wchar_t *command){
    HRESULT com=OleInitialize(nullptr);EverWindow d;d.owner=owner;d.source=source;d.dpi=dpi>0?dpi:96;
#ifdef LITELAUNCH_BATCH_PLUGIN
    d.batchLegacyAction=command&&*command;
#endif
    bool enabled=owner&&IsWindowEnabled(owner);
    try {
        Scintilla_RegisterClasses(GetModuleHandleW(nullptr));d.hidden=eSetting(L"HideExtension",L"0")==L"1";d.history=eLoadHistory();if(command&&std::wstring(command)==L"联动重命名"){for(auto path:eLines(eClipboardText(owner))){if(path.size()>1&&path.front()==L'"'&&path.back()==L'"')path=path.substr(1,path.size()-2);addPath(d.items,path);}}else eSourceSelection(source,d.items);
#ifdef LITELAUNCH_EVER_TESTING
        for(const auto&path:eTestPaths)addPath(d.items,path);
#endif
        d.sortInitial();WNDCLASSEXW wc={sizeof(wc)};wc.lpfnWndProc=EverWindow::proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=nullptr;wc.lpszClassName=L"LiteLaunch.Native.Ever.v1";wc.hIcon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1));wc.hIconSm=wc.hIcon;RegisterClassExW(&wc);
        RECT work={};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);int width=std::min(d.px(
#ifdef LITELAUNCH_BATCH_PLUGIN
1180
#else
920
#endif
),int(work.right-work.left)),height=std::min(d.px(740),int(work.bottom-work.top));if(enabled)EnableWindow(owner,FALSE);
        HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,eProductName(),WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,(work.left+work.right-width)/2,(work.top+work.bottom-height)/2,width,height,nullptr,nullptr,wc.hInstance,&d);
        if(h){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);SetFocus(d.editor);std::wstring action=command;if(action==L"后悔药")d.regret();else if(action==L"设置")d.settings();else if(action==L"设置联动修改")d.linked(true);else if(action==L"记忆功能")d.templates();else if(action.compare(0,5,L"记忆功能：")==0){auto ts=eTemplates();bool found=false;for(const auto&t:ts)if(t.name==action.substr(5)){if(d.apply(t.rule)&&d.execute())DestroyWindow(h);found=true;break;}if(!found)d.warn(L"没有找到此记忆模板。");}
        MSG msg;int got=1;while(IsWindow(h)&&(got=GetMessageW(&msg,nullptr,0,0))>0){if(msg.message==WM_KEYDOWN&&(GetKeyState(VK_CONTROL)&0x8000)&&(msg.wParam=='F'||msg.wParam=='H')){d.findReplace();continue;}if((msg.message==WM_KEYDOWN||msg.message==WM_SYSKEYDOWN)&&(GetKeyState(VK_MENU)&0x8000)&&(msg.wParam==VK_UP||msg.wParam==VK_DOWN)){d.sci(msg.wParam==VK_UP?SCI_MOVESELECTEDLINESUP:SCI_MOVESELECTEDLINESDOWN);continue;}if(msg.message==WM_KEYDOWN&&(GetKeyState(VK_CONTROL)&0x8000)&&(GetKeyState(VK_SHIFT)&0x8000)&&msg.wParam=='D'){d.sci(SCI_LINEDELETE);continue;}if(msg.message==WM_SYSKEYDOWN&&msg.wParam=='N'){d.naming();continue;}if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){DestroyWindow(h);continue;}if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(!got){if(IsWindow(h))DestroyWindow(h);PostQuitMessage((int)msg.wParam);}}
    }catch(const std::exception&){if(d.window&&IsWindow(d.window))DestroyWindow(d.window);MessageBoxW(owner,L"EVER重命名未能打开。请检查用户目录、历史记录和可用内存。",L"LiteLaunch",MB_ICONERROR);}
    ever::regexShutdown();if(enabled&&IsWindow(owner))EnableWindow(owner,TRUE);if(SUCCEEDED(com))OleUninitialize();
}
