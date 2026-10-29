#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "rename.h"
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <exdisp.h>
#include <servprov.h>
#include <shellapi.h>
#include <algorithm>
#include <cwctype>
#include <limits>
#include <regex>
#include <string>
#include <vector>
#include <functional>
#include <fstream>
#include <cstring>
#include <thread>
#include "ever_rules.h"
#include "batch_pipeline.h"
#include "ever_regex.h"
#include "vendor/scintilla/LiteLaunchEditor.h"

namespace llrename {
struct Identity { DWORD volume = 0, high = 0, low = 0; };
struct Entry {
    std::wstring path, name, planned, manual, status;
    bool folder = false, hasManual = false;
};
struct Options {
    std::wstring find, replace, prefix, suffix;
    bool regex = false, number = false, date = false, removeSpaces = false, keepExtension = true;
    long long start = 1, step = 1;
    int width = 3, letterCase = 0;
};
struct Move { std::wstring before, after; Identity identity; };
struct Result {
    bool ok = false;
    std::wstring error;
    std::vector<Move> undo;
};
static bool same(const std::wstring &a, const std::wstring &b) {
    return CompareStringOrdinal(a.c_str(), (int)a.size(), b.c_str(), (int)b.size(), TRUE) == CSTR_EQUAL;
}
static std::wstring fileName(const std::wstring &p) {
    size_t n = p.find_last_of(L"\\/"); return n == p.npos ? p : p.substr(n + 1);
}
static std::wstring parent(const std::wstring &p) {
    size_t n = p.find_last_of(L"\\/"); return n == p.npos ? L"" : p.substr(0, n + 1);
}
static std::wstring fullPath(const std::wstring &p) {
    DWORD n = GetFullPathNameW(p.c_str(), 0, nullptr, nullptr);
    if (!n) return L"";
    std::vector<wchar_t> b(n + 1);
    if (!GetFullPathNameW(p.c_str(), (DWORD)b.size(), b.data(), nullptr)) return L"";
    std::wstring r(b.data());
    while (r.size() > 3 && (r.back() == L'\\' || r.back() == L'/')) r.pop_back();
    return r;
}
static std::wstring nativePath(const std::wstring &p) {
    if (p.compare(0, 4, L"\\\\?\\") == 0) return p;
    if (p.compare(0, 2, L"\\\\") == 0) return L"\\\\?\\UNC\\" + p.substr(2);
    return L"\\\\?\\" + p;
}
static DWORD attrs(const std::wstring &p) { return GetFileAttributesW(nativePath(p).c_str()); }
static bool identifyNative(LPCWSTR path, Identity &id) {
    HANDLE h = CreateFileW(path, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION info = {};
    BOOL ok = GetFileInformationByHandle(h, &info); CloseHandle(h);
    if (ok) { id.volume = info.dwVolumeSerialNumber; id.high = info.nFileIndexHigh; id.low = info.nFileIndexLow; }
    return ok != FALSE;
}
static bool identify(const std::wstring &path, Identity &id) { return identifyNative(nativePath(path).c_str(), id); }
static bool sameIdentity(const Identity &a, const Identity &b) {
    return a.volume == b.volume && a.high == b.high && a.low == b.low;
}
static std::wstring winError(DWORD code) {
    LPWSTR p = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, (LPWSTR)&p, 0, nullptr);
    std::wstring r = p ? p : L"系统操作失败"; if (p) LocalFree(p);
    while (!r.empty() && (r.back() == L'\r' || r.back() == L'\n')) r.pop_back();
    return r + L"（" + std::to_wstring(code) + L"）";
}
static bool validName(const std::wstring &s, std::wstring &why) {
    if (s.empty() || s == L"." || s == L"..") why = L"名称不能为空或为点号";
    else if (s.size() > 255) why = L"名称超过255个字符";
    else if (s.back() == L' ' || s.back() == L'.') why = L"名称不能以空格或点号结尾";
    else {
        for (wchar_t c : s) if (c < 32 || std::wcschr(L"\\/:*?\"<>|", c)) { why = L"包含Windows非法字符"; break; }
        std::wstring base = s.substr(0, s.find(L'.'));
        while (!base.empty() && base.back() == L' ') base.pop_back();
        for (auto &c : base) c = (wchar_t)towupper(c);
        if (base == L"CON" || base == L"PRN" || base == L"AUX" || base == L"NUL" || base == L"CONIN$" || base == L"CONOUT$" ||
            (base.size() == 4 && (base.compare(0, 3, L"COM") == 0 || base.compare(0, 3, L"LPT") == 0) &&
             ((base[3] >= L'1' && base[3] <= L'9') || base[3] == L'\u00b9' || base[3] == L'\u00b2' || base[3] == L'\u00b3')))
            why = L"Windows保留名称";
    }
    return why.empty();
}
static bool addPath(std::vector<Entry> &items, const std::wstring &raw) {
    std::wstring p = fullPath(raw);
    if (p.empty() || p.size() < 4 || fileName(p).empty()) return false;
    DWORD a = attrs(p); if (a == INVALID_FILE_ATTRIBUTES) return false;
    for (const auto &e : items) if (same(e.path, p)) return false;
    Entry e; e.path = p; e.name = fileName(p); e.planned = e.name; e.folder = (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    items.push_back(e); return true;
}
static bool ancestor(const std::wstring &a, const std::wstring &b) {
    return b.size() > a.size() && b[a.size()] == L'\\' && same(a, b.substr(0, a.size()));
}
static bool makePreview(std::vector<Entry> &items, const Options &o, std::wstring &error) {
    std::wregex re;
    if (o.regex && !o.find.empty()) {
        try { re.assign(o.find, std::regex_constants::ECMAScript); }
        catch (const std::regex_error &) { error = L"正则表达式无效，请修改查找内容。"; return false; }
    }
    if (o.start < 0 || o.step < 1 || o.width < 0 || o.width > 16 || o.start > 2147483647LL || o.step > 2147483647LL) {
        error = L"编号起始值应为0—2147483647，步长为1—2147483647，位数为0—16。"; return false;
    }
    SYSTEMTIME now; GetLocalTime(&now);
    wchar_t day[32]; swprintf(day, 32, L"%04u%02u%02u_", now.wYear, now.wMonth, now.wDay);
    for (size_t i = 0; i < items.size(); ++i) {
        Entry &e = items[i]; e.status.clear();
        if (e.hasManual) { e.planned = e.manual; continue; }
        std::wstring stem = e.name, ext;
        size_t dot = stem.find_last_of(L'.');
        if (o.keepExtension && !e.folder && dot != stem.npos && dot > 0) { ext = stem.substr(dot); stem.resize(dot); }
        if (!o.find.empty()) {
            if (o.regex) {
                try { stem = std::regex_replace(stem, re, o.replace); }
                catch (const std::regex_error &) { error = L"正则匹配过于复杂或替换失败，请简化表达式。"; return false; }
            }
            else { size_t at = 0; while ((at = stem.find(o.find, at)) != stem.npos) { stem.replace(at, o.find.size(), o.replace); at += o.replace.size(); } }
        }
        if (o.removeSpaces) stem.erase(std::remove_if(stem.begin(), stem.end(), [](wchar_t c) { return iswspace(c) != 0; }), stem.end());
        if (o.letterCase) {
            DWORD flag = o.letterCase == 1 ? LCMAP_UPPERCASE : LCMAP_LOWERCASE;
            int n = LCMapStringEx(LOCALE_NAME_INVARIANT, flag, stem.c_str(), (int)stem.size(), nullptr, 0, nullptr, nullptr, 0);
            if (n) { std::wstring out((size_t)n, 0); LCMapStringEx(LOCALE_NAME_INVARIANT, flag, stem.c_str(), (int)stem.size(), &out[0], n, nullptr, nullptr, 0); stem = out; }
        }
        std::wstring num;
        if (o.number) {
            long long value = o.start + (long long)i * o.step;
            num = std::to_wstring(value);
            if ((int)num.size() < o.width) num.insert(0, (size_t)(o.width - num.size()), L'0');
            num = L"_" + num;
        }
        e.planned = o.prefix + (o.date ? day : L"") + stem + o.suffix + num + ext;
    }
    return true;
}
static bool validate(const std::vector<Move> &moves, std::wstring &error, bool checkIdentity = true) {
    for (size_t i = 0; i < moves.size(); ++i) {
        const Move &m = moves[i];
        std::wstring why;
        if (parent(m.before) != parent(m.after) || !validName(fileName(m.after), why)) {
            error = fileName(m.after) + L"：" + (why.empty() ? L"只能修改名称，不能改变所在目录" : why); return false;
        }
        DWORD a = attrs(m.before);
        if (a == INVALID_FILE_ATTRIBUTES) { error = L"源文件不可访问：" + m.before; return false; }
        if (checkIdentity) { Identity id; if (!identify(m.before, id) || !sameIdentity(id, m.identity)) { error = L"源文件已变化或无法核对：" + m.before; return false; } }
        for (size_t j = 0; j < i; ++j) {
            if (same(m.before, moves[j].before)) { error = L"源路径重复：" + m.before; return false; }
            if (same(m.after, moves[j].after)) { error = L"目标名称重复（含大小写重复）：" + m.after; return false; }
            if (ancestor(m.before, moves[j].before) || ancestor(moves[j].before, m.before)) { error = L"不能同时修改文件夹及其内部项目，请分批操作。"; return false; }
        }
        DWORD target = attrs(m.after);
        if (target != INVALID_FILE_ATTRIBUTES) {
            bool selected = false; for (const auto &other : moves) if (same(m.after, other.before)) { selected = true; break; }
            if (!selected) { error = L"目标已存在，已阻止整批操作：" + m.after; return false; }
        } else {
            DWORD code = GetLastError();
            if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND) { error = L"无法检查目标是否存在：" + m.after + L"\n" + winError(code); return false; }
        }
    }
    return true;
}
#ifdef LITELAUNCH_RENAME_TESTING
static int testMoveCall=0;
static std::vector<int> testFailedMoves;
#endif
static bool moveNative(LPCWSTR from, LPCWSTR to, const Identity &identity, DWORD &error) {
#ifdef LITELAUNCH_RENAME_TESTING
    if(std::find(testFailedMoves.begin(),testFailedMoves.end(),++testMoveCall)!=testFailedMoves.end()) { error=ERROR_ACCESS_DENIED; return false; }
#endif
    Identity now;
    if (!identifyNative(from, now) || !sameIdentity(now, identity)) { error = ERROR_FILE_INVALID; return false; }
    if (MoveFileExW(from, to, MOVEFILE_WRITE_THROUGH)) return true;
    error = GetLastError(); return false;
}
/* During a failed undo, retain the original destination but update its actual current path. */
static void reconcileUndo(std::vector<Move> &history, const std::vector<Move> &current) {
    for (auto &original : history) for (const auto &change : current)
        if (same(original.after, change.before) && sameIdentity(original.identity, change.identity)) {
            original.after = change.after; break;
        }
    history.erase(std::remove_if(history.begin(), history.end(), [](const Move &m) { return m.before == m.after; }), history.end());
}
static Result transact(const std::vector<Move> &input) {
    Result r;
    if (!validate(input, r.error)) return r;
    std::vector<Move> moves;
    for (const auto &m : input) if (m.before != m.after) moves.push_back(m);
    if (moves.empty()) { r.error = L"名称没有变化。"; return r; }
    std::vector<std::wstring> temp, nativeBefore, nativeAfter, nativeTemp;
    std::vector<int> stage(moves.size(), 0); // 0 original, 1 temporary, 2 final
    r.undo = moves;
    for (size_t i = 0; i < moves.size(); ++i) {
        std::wstring t; int attempts = 0;
        do {
            t = parent(moves[i].before) + L".~LiteLaunch-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(i) + L"-" + std::to_wstring(attempts++);
        } while (attrs(t) != INVALID_FILE_ATTRIBUTES && attempts < 1000);
        if (attrs(t) != INVALID_FILE_ATTRIBUTES) { r.undo.clear(); r.error = L"无法创建唯一临时名称。"; return r; }
        temp.push_back(t);
        nativeBefore.push_back(nativePath(moves[i].before)); nativeAfter.push_back(nativePath(moves[i].after)); nativeTemp.push_back(nativePath(t));
        r.undo[i].after.reserve(std::max(t.size(),std::max(moves[i].before.size(),moves[i].after.size())));
    }
    DWORD fail = ERROR_SUCCESS; size_t failedIndex = 0;
    for (size_t i = 0; i < moves.size(); ++i) {
        if (!moveNative(nativeBefore[i].c_str(), nativeTemp[i].c_str(), moves[i].identity, fail)) { failedIndex = i; break; }
        stage[i] = 1;
    }
    if (!fail) {
        for (size_t i = 0; i < moves.size(); ++i) {
            if (!moveNative(nativeTemp[i].c_str(), nativeAfter[i].c_str(), moves[i].identity, fail)) { failedIndex = i; break; }
            stage[i] = 2;
        }
    }
    if (!fail) { r.ok = true; return r; }
    // Return finalized targets to their individual temporary names before restoring originals.
    for (size_t i = 0; i < moves.size(); ++i) if (stage[i] == 2) {
        DWORD ignored; if (moveNative(nativeAfter[i].c_str(), nativeTemp[i].c_str(), moves[i].identity, ignored)) stage[i] = 1;
    }
    for (size_t i = 0; i < moves.size(); ++i) if (stage[i] != 0) {
        DWORD ignored; LPCWSTR current=stage[i]==1?nativeTemp[i].c_str():nativeAfter[i].c_str();
        if (moveNative(current, nativeBefore[i].c_str(), moves[i].identity, ignored)) stage[i] = 0;
    }
    // All path storage, native paths and undo capacity were allocated before the first move.
    // Rollback never allocates; error-message formatting happens only after rollback finishes.
    for (size_t i = 0; i < moves.size(); ++i) r.undo[i].after = stage[i]==0?moves[i].before:stage[i]==1?temp[i]:moves[i].after;
    r.undo.erase(std::remove_if(r.undo.begin(),r.undo.end(),[](const Move&m){return m.before==m.after;}),r.undo.end());
    r.error = L"重命名失败：" + fileName(moves[failedIndex].before) + L"\n" + winError(fail);
    for (const auto &u : r.undo) r.error += L"\n未能还原：" + u.before + L"\n当前路径：" + u.after;
    r.error += r.undo.empty() ? L"\n已还原本批全部名称。" : L"\n请保留本窗口，可尝试撤销；不要移动上面列出的项目。";
    return r;
}
static bool makeMoves(const std::vector<Entry> &items, std::vector<Move> &moves, std::wstring &error) {
    for (const auto &e : items) {
        Move m; m.before = e.path; m.after = parent(e.path) + e.planned;
        if (!identify(e.path, m.identity)) { error = L"无法核对源文件：" + e.path; return false; }
        moves.push_back(m);
    }
    return validate(moves, error);
}

template<class T> struct Com {
    T *p = nullptr;
    ~Com() { if (p) p->Release(); }
    T *operator->() const { return p; }
    T **out() { return &p; }
};
using SelectionQuery = HRESULT (*)(void *,BOOL,IShellItemArray **);
static HRESULT querySelection(void *context,BOOL includeParent,IShellItemArray **selection) {
    return ((IFolderView2*)context)->GetSelection(includeParent,selection);
}
static bool collectExplicitSelection(SelectionQuery query,void *context,std::vector<Entry>&entries) {
    Com<IShellItemArray> selected;
    // FALSE is essential: TRUE substitutes the containing folder when nothing is selected.
    if(FAILED(query(context,FALSE,selected.out())) || !selected.p) return false;
    DWORD count=0;selected->GetCount(&count);size_t before=entries.size();
    for(DWORD i=0;i<count;++i) { Com<IShellItem> item;LPWSTR p=nullptr;
        if(SUCCEEDED(selected->GetItemAt(i,item.out())) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&p))) { addPath(entries,p);CoTaskMemFree(p); }
    }
    return entries.size()!=before;
}
static bool fromBrowser(IDispatch *disp, HWND source, bool desktop, std::vector<Entry> &entries) {
    Com<IWebBrowserApp> browser;
    if (FAILED(disp->QueryInterface(IID_PPV_ARGS(browser.out())))) return false;
    SHANDLE_PTR hwnd = 0; browser->get_HWND(&hwnd);
    HWND target = GetAncestor(source, GA_ROOT);
    if (!desktop && (HWND)hwnd != target && (HWND)hwnd != source) return false;
    Com<IServiceProvider> provider; Com<IShellBrowser> shell; Com<IShellView> view; Com<IFolderView2> folder;
    if (FAILED(browser->QueryInterface(IID_PPV_ARGS(provider.out()))) ||
        FAILED(provider->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(shell.out()))) ||
        FAILED(shell->QueryActiveShellView(view.out())) || FAILED(view->QueryInterface(IID_PPV_ARGS(folder.out())))) return false;
    return collectExplicitSelection(querySelection,folder.p,entries);
}
static bool readSelection(HWND source, std::vector<Entry> &entries) {
    if (!source || !IsWindow(source)) return false;
    Com<IShellWindows> windows;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(windows.out())))) return false;
    long count = 0; windows->get_Count(&count);
    for (long i = 0; i < count; ++i) {
        VARIANT index; VariantInit(&index); index.vt = VT_I4; index.lVal = i;
        Com<IDispatch> disp;
        if (SUCCEEDED(windows->Item(index, disp.out())) && disp.p && fromBrowser(disp.p, source, false, entries)) return true;
    }
    wchar_t cls[80] = {}; GetClassNameW(GetAncestor(source, GA_ROOT), cls, 80);
    if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW")) {
        VARIANT loc, root; VariantInit(&loc); VariantInit(&root); loc.vt = VT_I4; loc.lVal = CSIDL_DESKTOP;
        long hwnd = 0; Com<IDispatch> desktop;
        if (SUCCEEDED(windows->FindWindowSW(&loc, &root, SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, desktop.out())) && desktop.p)
            return fromBrowser(desktop.p, source, true, entries);
    }
    return false;
}

enum { FILES = 100, FOLDERS, READ, REMOVE, SORT, CLEAR, LIST, FIND, REPLACE, REGEX,
    PREFIX, SUFFIX, NUMBER, START, STEP, WIDTH, DATE, CASE, SPACE, EXT, MANUAL, SETMANUAL, AUTO, EXECUTE, UNDO, STATUS, CLOSE };
struct Dialog {
    HWND window = nullptr, owner = nullptr, source = nullptr;
    HFONT font = nullptr; HBRUSH background = nullptr;
    int dpi = 96; bool updating = false, ascending = true, previewOk = false;
    std::vector<Entry> items;
    std::vector<Move> undo;
    std::wstring previewError;
    int px(int n) const { return MulDiv(n, dpi, 96); }
    HWND ctl(int id) const { return GetDlgItem(window, id); }
    std::wstring text(int id) const { int n = GetWindowTextLengthW(ctl(id)); std::wstring t((size_t)n + 1, 0); GetWindowTextW(ctl(id), &t[0], n + 1); t.resize(n); return t; }
    bool checked(int id) const { return SendMessageW(ctl(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }
    HWND child(int id, const wchar_t *cls, const wchar_t *label, DWORD style) {
        HWND h = CreateWindowExW(!wcscmp(cls, L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, label, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, window, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(h, WM_SETFONT, (WPARAM)font, TRUE); return h;
    }
    void position(int id, int x, int y, int w, int h) { MoveWindow(ctl(id), px(x), px(y), px(w), px(h), TRUE); }
    void label(int id, const wchar_t *t) { child(id, L"STATIC", t, SS_LEFT); }
    void controls() {
        font = CreateFontW(-px(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
        background = CreateSolidBrush(RGB(248, 250, 253));
        child(FILES,L"BUTTON",L"添加文件",WS_TABSTOP); child(FOLDERS,L"BUTTON",L"添加文件夹",WS_TABSTOP);
        child(READ,L"BUTTON",L"读取选中项",WS_TABSTOP); child(REMOVE,L"BUTTON",L"移除所选",WS_TABSTOP);
        child(SORT,L"BUTTON",L"名称排序 ↑",WS_TABSTOP); child(CLEAR,L"BUTTON",L"清空列表",WS_TABSTOP);
        child(LIST,WC_LISTVIEWW,L"",LVS_REPORT | LVS_SHOWSELALWAYS | WS_TABSTOP);
        ListView_SetExtendedListViewStyle(ctl(LIST), LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        const wchar_t *headers[] = {L"原名称", L"预览新名称", L"所在目录", L"状态"};
        for (int i = 0; i < 4; ++i) { LVCOLUMNW c = {}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = (LPWSTR)headers[i]; c.cx = px(i == 3 ? 120 : 220); ListView_InsertColumn(ctl(LIST), i, &c); }
        label(200,L"查找"); label(201,L"替换"); label(202,L"前缀"); label(203,L"后缀");
        for (int id : { FIND,REPLACE,PREFIX,SUFFIX,START,STEP,WIDTH,MANUAL }) { child(id,L"EDIT",L"",WS_TABSTOP | ES_AUTOHSCROLL); SendMessageW(ctl(id),EM_SETLIMITTEXT,id == MANUAL ? 255 : 512,0); }
        child(REGEX,L"BUTTON",L"正则表达式（替换可用$1）",BS_AUTOCHECKBOX | WS_TABSTOP);
        child(NUMBER,L"BUTTON",L"编号后缀",BS_AUTOCHECKBOX | WS_TABSTOP);
        label(204,L"起始"); label(205,L"步长"); label(206,L"位数");
        SetWindowTextW(ctl(START),L"1"); SetWindowTextW(ctl(STEP),L"1"); SetWindowTextW(ctl(WIDTH),L"3");
        child(DATE,L"BUTTON",L"日期前缀（年月日）",BS_AUTOCHECKBOX | WS_TABSTOP);
        child(CASE,L"COMBOBOX",L"",CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
        for (auto v : {L"大小写保持",L"转大写",L"转小写"}) SendMessageW(ctl(CASE),CB_ADDSTRING,0,(LPARAM)v);
        SendMessageW(ctl(CASE),CB_SETCURSEL,0,0);
        child(SPACE,L"BUTTON",L"去除空白字符",BS_AUTOCHECKBOX | WS_TABSTOP);
        child(EXT,L"BUTTON",L"保留文件扩展名",BS_AUTOCHECKBOX | WS_TABSTOP); SendMessageW(ctl(EXT),BM_SETCHECK,BST_CHECKED,0);
        label(207,L"手工完整新名（选中一行，包含扩展名）");
        child(SETMANUAL,L"BUTTON",L"应用到所选行",WS_TABSTOP); child(AUTO,L"BUTTON",L"恢复自动",WS_TABSTOP);
        label(208,L"仅处理列表中明确选择的项目；预览不会修改文件。");
        child(EXECUTE,L"BUTTON",L"执行重命名",WS_TABSTOP); child(UNDO,L"BUTTON",L"撤销上次",WS_TABSTOP);
        child(CLOSE,L"BUTTON",L"关闭",WS_TABSTOP); label(STATUS,L"");
        DragAcceptFiles(window, TRUE);
    }
    void layout() {
        RECT rc; GetClientRect(window,&rc); int w = MulDiv(rc.right,96,dpi), h = MulDiv(rc.bottom,96,dpi);
        int bottom = h - 255;
        position(FILES,16,14,88,30); position(FOLDERS,112,14,100,30); position(READ,220,14,102,30);
        position(REMOVE,330,14,90,30); position(SORT,428,14,110,30); position(CLEAR,546,14,90,30);
        position(LIST,16,54,w-32,bottom-64);
        int half=(w-48)/2;
        position(200,16,bottom+4,40,24); position(FIND,60,bottom,half-44,28);
        position(201,half+32,bottom+4,40,24); position(REPLACE,half+76,bottom,half-44,28);
        position(202,16,bottom+40,40,24); position(PREFIX,60,bottom+36,half-44,28);
        position(203,half+32,bottom+40,40,24); position(SUFFIX,half+76,bottom+36,half-44,28);
        position(REGEX,16,bottom+74,236,26); position(NUMBER,268,bottom+74,92,26);
        position(204,370,bottom+78,34,24); position(START,406,bottom+72,70,28);
        position(205,484,bottom+78,34,24); position(STEP,520,bottom+72,65,28);
        position(206,593,bottom+78,34,24); position(WIDTH,629,bottom+72,48,28);
        position(DATE,16,bottom+108,192,26); position(CASE,216,bottom+108,136,180);
        position(SPACE,368,bottom+108,154,26); position(EXT,536,bottom+108,180,26);
        position(207,16,bottom+144,300,24); position(MANUAL,322,bottom+140,w-554,28);
        position(SETMANUAL,w-224,bottom+140,116,28); position(AUTO,w-100,bottom+140,84,28);
        position(208,16,bottom+179,w-32,24); position(STATUS,16,h-38,w-348,26);
        position(EXECUTE,w-322,h-42,118,32); position(UNDO,w-196,h-42,90,32); position(CLOSE,w-98,h-42,82,32);
        int avail = rc.right - px(32); ListView_SetColumnWidth(ctl(LIST),0,avail*24/100);
        ListView_SetColumnWidth(ctl(LIST),1,avail*28/100); ListView_SetColumnWidth(ctl(LIST),2,avail*33/100); ListView_SetColumnWidth(ctl(LIST),3,avail*15/100);
    }
    int selected() { return ListView_GetNextItem(ctl(LIST),-1,LVNI_SELECTED); }
    bool parse(int id,long long &n) {
        std::wstring t=text(id); if (t.empty()) return false;
        wchar_t *end=nullptr; n=wcstoll(t.c_str(),&end,10); return end && !*end && n>=0 && n<=2147483647LL;
    }
    void preview() {
        if (updating) return; updating=true;
        Options o; o.find=text(FIND); o.replace=text(REPLACE); o.prefix=text(PREFIX); o.suffix=text(SUFFIX);
        o.regex=checked(REGEX); o.number=checked(NUMBER); o.date=checked(DATE); o.removeSpaces=checked(SPACE); o.keepExtension=checked(EXT);
        o.letterCase=(int)SendMessageW(ctl(CASE),CB_GETCURSEL,0,0);
        long long width=0; previewError.clear();
        previewOk=parse(START,o.start) && parse(STEP,o.step) && parse(WIDTH,width) && width<=16;
        o.width=(int)width;
        if (previewOk) previewOk=makePreview(items,o,previewError);
        else previewError=L"编号参数无效，请输入合法数字。";
        std::vector<Move> moves;
        if (previewOk && !items.empty()) previewOk=makeMoves(items,moves,previewError);
        int sel=selected(); ListView_DeleteAllItems(ctl(LIST));
        for (size_t i=0;i<items.size();++i) {
            Entry &e=items[i]; std::wstring reason;
            if (e.planned==e.name) e.status=L"不变";
            else if (!validName(e.planned,reason)) e.status=reason;
            else e.status=e.hasManual ? L"手工名称" : L"待重命名";
            LVITEMW row={}; row.mask=LVIF_TEXT; row.iItem=(int)i; row.pszText=(LPWSTR)e.name.c_str(); ListView_InsertItem(ctl(LIST),&row);
            ListView_SetItemText(ctl(LIST),(int)i,1,(LPWSTR)e.planned.c_str());
            std::wstring dir=parent(e.path); ListView_SetItemText(ctl(LIST),(int)i,2,(LPWSTR)dir.c_str()); ListView_SetItemText(ctl(LIST),(int)i,3,(LPWSTR)e.status.c_str());
        }
        if (sel>=0 && sel<(int)items.size()) ListView_SetItemState(ctl(LIST),sel,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
        bool changed=false; for(const auto&e:items) if(e.planned!=e.name) changed=true;
        EnableWindow(ctl(EXECUTE),previewOk && changed); EnableWindow(ctl(UNDO),!undo.empty());
        std::wstring msg=items.empty()?L"请选择文件或文件夹。": previewOk?std::to_wstring(items.size())+L"项，预览有效":previewError;
        SetWindowTextW(ctl(STATUS),msg.c_str()); updating=false;
    }
    void choose(bool folders) {
        Com<IFileOpenDialog> pick;
        HRESULT hr=CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(pick.out()));
        if(FAILED(hr)) { MessageBoxW(window,L"文件选择器不可用，可以直接拖入文件或文件夹。",L"批量重命名",MB_ICONERROR); return; }
        DWORD flags=0; pick->GetOptions(&flags); pick->SetOptions(flags | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | (folders?FOS_PICKFOLDERS:0));
        pick->SetTitle(folders?L"选择要重命名的文件夹（不扫描内部文件）":L"选择要重命名的文件");
        if(SUCCEEDED(pick->Show(window))) {
            Com<IShellItemArray> result; if(SUCCEEDED(pick->GetResults(result.out()))) {
                DWORD n=0; result->GetCount(&n); for(DWORD i=0;i<n;++i) { Com<IShellItem> item; LPWSTR p=nullptr;
                    if(SUCCEEDED(result->GetItemAt(i,item.out())) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&p))) { addPath(items,p); CoTaskMemFree(p); } }
            }
        }
        preview();
    }
    void updatePaths(const std::vector<Move>&changes) {
        for(auto&e:items) for(const auto&m:changes) if(same(e.path,m.before)) { e.path=m.after; e.name=fileName(e.path); e.hasManual=false; break; }
    }
    void execute(bool reverse) {
        std::vector<Move> moves; std::wstring error;
        if(reverse) { for(const auto&m:undo) { Move r=m; r.before=m.after; r.after=m.before; moves.push_back(r); } }
        else { preview(); if(!previewOk || !makeMoves(items,moves,error)) { MessageBoxW(window,(error.empty()?previewError:error).c_str(),L"无法重命名",MB_ICONWARNING); return; } }
        if(!validate(moves,error)) { MessageBoxW(window,error.c_str(),L"操作已阻止",MB_ICONWARNING); return; }
        size_t n=0; for(const auto&m:moves) if(m.before!=m.after) ++n;
        std::wstring prompt=(reverse?L"撤销上次的":L"将重命名")+std::to_wstring(n)+L"个项目。\n已检查名称和冲突，不会覆盖文件。\n是否执行？";
        if(MessageBoxW(window,prompt.c_str(),reverse?L"确认撤销":L"确认重命名",MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2)!=IDYES) return;
        SetCursor(LoadCursorW(nullptr,IDC_WAIT)); Result r=transact(moves); SetCursor(LoadCursorW(nullptr,IDC_ARROW));
        if(r.ok) {
            updatePaths(r.undo); if(reverse) undo.clear(); else undo=r.undo;
            // Clear transforms after a completed batch so the next preview cannot silently apply them twice.
            updating=true; for(int id:{FIND,REPLACE,PREFIX,SUFFIX}) SetWindowTextW(ctl(id),L"");
            for(int id:{REGEX,NUMBER,DATE,SPACE}) SendMessageW(ctl(id),BM_SETCHECK,BST_UNCHECKED,0);
            SendMessageW(ctl(CASE),CB_SETCURSEL,0,0); updating=false; preview();
            std::wstring done=reverse?L"已撤销上次重命名。":L"重命名完成。可在关闭本窗口前撤销上次操作。";
            MessageBoxW(window,done.c_str(),L"批量重命名",MB_ICONINFORMATION);
        } else {
            if(!r.undo.empty()) { updatePaths(r.undo); if(reverse) reconcileUndo(undo,r.undo); else undo=r.undo; }
            preview(); MessageBoxW(window,r.error.c_str(),L"操作失败",MB_ICONERROR);
        }
    }
};
static LRESULT CALLBACK dispatch(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    Dialog *d=(Dialog*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if(msg==WM_NCCREATE) { d=(Dialog*)((CREATESTRUCTW*)lp)->lpCreateParams; d->window=hwnd; SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)d); }
    if(!d) return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg) {
    case WM_CREATE: d->controls(); d->layout(); d->preview(); return 0;
    case WM_SIZE: if(d->ctl(LIST)) d->layout(); return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO*)lp)->ptMinTrackSize={d->px(840),d->px(570)}; return 0;
    case WM_ERASEBKGND: { RECT r; GetClientRect(hwnd,&r); FillRect((HDC)wp,&r,d->background); return 1; }
    case WM_CTLCOLORSTATIC: SetBkMode((HDC)wp,TRANSPARENT); SetTextColor((HDC)wp,RGB(40,47,58)); return (LRESULT)d->background;
    case WM_COMMAND: {
        int id=LOWORD(wp),code=HIWORD(wp);
        if(code==EN_CHANGE && !d->updating && id!=MANUAL) { SetTimer(hwnd,1,120,nullptr); return 0; }
        if(id==FILES) d->choose(false); else if(id==FOLDERS) d->choose(true);
        else if(id==READ) { if(!readSelection(d->source,d->items)) MessageBoxW(hwnd,L"没有读取到原资源管理器窗口中的选中项。请用添加按钮或拖入项目。",L"批量重命名",MB_ICONINFORMATION); d->preview(); }
        else if(id==REMOVE) { for(int i=(int)d->items.size()-1;i>=0;--i) if(ListView_GetItemState(d->ctl(LIST),i,LVIS_SELECTED)) d->items.erase(d->items.begin()+i); d->preview(); }
        else if(id==CLEAR) { d->items.clear(); d->preview(); }
        else if(id==SORT) { bool ascending=d->ascending; std::stable_sort(d->items.begin(),d->items.end(),[ascending](const Entry&a,const Entry&b) { int n=StrCmpLogicalW(a.name.c_str(),b.name.c_str()); if(!n)n=StrCmpLogicalW(a.path.c_str(),b.path.c_str()); return ascending?n<0:n>0; }); d->ascending=!ascending; SetWindowTextW(d->ctl(SORT),d->ascending?L"名称排序 ↑":L"名称排序 ↓"); d->preview(); }
        else if(id==SETMANUAL) { int sel=d->selected(); if(sel>=0) { d->items[sel].manual=d->text(MANUAL); d->items[sel].hasManual=true; d->preview(); } }
        else if(id==AUTO) { int sel=d->selected(); if(sel>=0) { d->items[sel].hasManual=false; d->preview(); } }
        else if(id==EXECUTE) d->execute(false); else if(id==UNDO) d->execute(true);
        else if(id==CLOSE || id==IDCANCEL) DestroyWindow(hwnd);
        else if((code==BN_CLICKED && id>=REGEX && id<=EXT) || (id==CASE && code==CBN_SELCHANGE)) d->preview();
        return 0;
    }
    case WM_TIMER: KillTimer(hwnd,1); d->preview(); return 0;
    case WM_NOTIFY: if(((NMHDR*)lp)->idFrom==LIST && ((NMHDR*)lp)->code==LVN_ITEMCHANGED && !d->updating) {
        int sel=d->selected(); SetWindowTextW(d->ctl(MANUAL),sel>=0?d->items[sel].planned.c_str():L"");
    } return 0;
    case WM_DROPFILES: { HDROP drop=(HDROP)wp; UINT n=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);
        for(UINT i=0;i<n;++i) { UINT len=DragQueryFileW(drop,i,nullptr,0); std::wstring p((size_t)len+1,0); DragQueryFileW(drop,i,&p[0],len+1); p.resize(len); addPath(d->items,p); }
        DragFinish(drop); d->preview(); return 0; }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: KillTimer(hwnd,1); DragAcceptFiles(hwnd,FALSE); return 0;
    case WM_NCDESTROY: if(d->font)DeleteObject(d->font); if(d->background)DeleteObject(d->background); d->font=nullptr; d->background=nullptr; d->window=nullptr; break;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    try { return dispatch(hwnd,msg,wp,lp); }
    catch(const std::exception &) {
        Dialog *d=(Dialog*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
        if(d) { d->updating=false; d->previewOk=false; KillTimer(hwnd,1); EnableWindow(d->ctl(EXECUTE),FALSE); }
        MessageBoxW(hwnd,L"当前预览遇到错误，执行已停止。请简化正则表达式或减少所选项目。",L"批量重命名",MB_ICONERROR);
        return msg==WM_CREATE ? -1 : 0;
    }
}
#ifndef LITELAUNCH_RENAME_TESTING
#include "ever_window.inc"
#endif
} // namespace llrename

#ifdef LITELAUNCH_RENAME_TESTING
static bool renameTestThrowAfterCreate=false;
#endif
extern "C" void renameShow(HWND owner,HWND sourceWindow,int dpi) {
#ifndef LITELAUNCH_RENAME_TESTING
    llrename::everShow(owner,sourceWindow,dpi,L"");
    return;
#else
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    llrename::Dialog d; d.owner=owner; d.source=sourceWindow; d.dpi=dpi>0?dpi:96;
    bool ownerEnabled=owner && IsWindowEnabled(owner);
    try {
        INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&controls);
        WNDCLASSEXW wc={}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=llrename::proc; wc.hInstance=GetModuleHandleW(nullptr);
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.lpszClassName=L"LiteLaunch.Native.Rename.v1";
        RegisterClassExW(&wc);
        llrename::readSelection(sourceWindow,d.items);
        int w=d.px(940),h=d.px(670); RECT screen={}; SystemParametersInfoW(SPI_GETWORKAREA,0,&screen,0);
        w=std::min(w,(int)(screen.right-screen.left)); h=std::min(h,(int)(screen.bottom-screen.top));
        int x=screen.left+(screen.right-screen.left-w)/2,y=screen.top+(screen.bottom-screen.top-h)/2;
        if(ownerEnabled)EnableWindow(owner,FALSE);
        HWND dialog=CreateWindowExW(WS_EX_DLGMODALFRAME,L"LiteLaunch.Native.Rename.v1",L"批量重命名",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,x,y,w,h,owner,nullptr,wc.hInstance,&d);
#ifdef LITELAUNCH_RENAME_TESTING
        if(renameTestThrowAfterCreate && dialog) throw std::bad_alloc();
#endif
        if(dialog) {
            ShowWindow(dialog,SW_SHOWNORMAL); SetForegroundWindow(dialog); SetFocus(d.ctl(llrename::FILES));
            MSG msg; int got=0;
            while(IsWindow(dialog) && (got=GetMessageW(&msg,nullptr,0,0))>0) {
                if(!IsDialogMessageW(dialog,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            }
            if(got==0 && IsWindow(dialog)) { DestroyWindow(dialog); PostQuitMessage((int)msg.wParam); }
        } else MessageBoxW(owner,L"无法打开重命名窗口。",L"LiteLaunch",MB_ICONERROR);
    } catch(const std::exception &) {
        if(d.window && IsWindow(d.window))DestroyWindow(d.window);
        MessageBoxW(owner,L"重命名窗口遇到错误，操作已停止。请检查文件名称和可用内存。",L"LiteLaunch",MB_ICONERROR);
    }
    if(ownerEnabled && IsWindow(owner))EnableWindow(owner,TRUE);
    if(SUCCEEDED(com))CoUninitialize();
#endif
}
#ifndef LITELAUNCH_RENAME_TESTING
extern "C" void renameShowCommand(HWND owner,HWND sourceWindow,int dpi,LPCWSTR command) {
    llrename::everShow(owner,sourceWindow,dpi,command?command:L"");
}
extern "C" HWND renameActiveWindow(void) {
    HWND window=llrename::eCurrentWindow;
    if(!window||!IsWindow(window))return nullptr;
    HWND popup=GetLastActivePopup(window);
    return popup&&IsWindowVisible(popup)?popup:window;
}
#else
extern "C" HWND renameActiveWindow(void) { return nullptr; }
#endif
