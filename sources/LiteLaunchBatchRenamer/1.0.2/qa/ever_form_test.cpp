// In-process production form binding regression. No external windows or input.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <cstdio>
#include <string>
#include <functional>
#include <stdexcept>

static bool qaPrivateForm = false;
static int qaWarnings = 0;
static int WINAPI qaMessageBox(HWND, LPCWSTR, LPCWSTR, UINT) { ++qaWarnings; return IDNO; }
static bool qaOwnForm(HWND window) {
    DWORD process = 0; const DWORD thread = GetWindowThreadProcessId(window, &process);
    wchar_t name[80] = {}; GetClassNameW(window, name, 80);
    return process == GetCurrentProcessId() && thread == GetCurrentThreadId() && !wcscmp(name, L"LiteLaunch.Ever.Form");
}
static BOOL WINAPI qaShowWindow(HWND window, int show) {
    return ::ShowWindow(window, qaPrivateForm && qaOwnForm(window) ? SW_HIDE : show);
}
#define MessageBoxW qaMessageBox
#define ShowWindow qaShowWindow
#define LITELAUNCH_EVER_TESTING
#include "../rename.cpp"
#undef MessageBoxW
#undef ShowWindow

using namespace llrename;
namespace {
int passed = 0, failed = 0;
HHOOK creationHook = nullptr;
HWND currentForm = nullptr;
bool callbackRan = false;
std::string callbackError;
std::function<void(EForm &)> formAction;
constexpr UINT_PTR formTimer = 0x45565246;
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
void check(bool condition, const char *name) { std::printf("%s %s\n", condition ? "PASS" : "FAIL", name); condition ? ++passed : ++failed; }
bool fieldVisible(EForm &form, int id) {
    for (const auto &field : form.fields) if (field.id == id)
        return field.visible && (GetWindowLongPtrW(form.ctl(id), GWL_STYLE) & WS_VISIBLE) != 0;
    throw std::runtime_error("Expected own form field missing");
}
void choose(EForm &form, int id, const wchar_t *value) {
    HWND control = form.ctl(id);
    int index = (int)SendMessageW(control, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)value);
    require(index != CB_ERR, "Expected own combo choice missing");
    require(SendMessageW(control, CB_SETCURSEL, index, 0) != CB_ERR, "Own combo selection failed");
    SendMessageW(form.window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), (LPARAM)control);
}
void editCombo(EForm &form, int id, const wchar_t *value) {
    COMBOBOXINFO info{sizeof(info)};
    require(GetComboBoxInfo(form.ctl(id), &info) && info.hwndItem, "Editable own combo unavailable");
    wchar_t type[30] = {}; GetClassNameW(info.hwndItem, type, 30);
    require(!_wcsicmp(type, L"Edit"), "Own source combo is not editable");
    // EM_REPLACESEL exercises the edit->combo notification path without keyboard input.
    SendMessageW(info.hwndItem, EM_SETSEL, 0, -1);
    SendMessageW(info.hwndItem, EM_REPLACESEL, TRUE, (LPARAM)value);
    SendMessageW(form.window, WM_COMMAND, MAKEWPARAM(id, CBN_EDITCHANGE), (LPARAM)form.ctl(id));
}
void tick(EForm &form, int id, bool value) {
    SendMessageW(form.ctl(id), BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(form.window, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)form.ctl(id));
}
void CALLBACK fillForm(HWND window, UINT, UINT_PTR timer, DWORD) {
    KillTimer(window, timer);
    if (!qaOwnForm(window) || window != currentForm) { callbackError = "Timer target is not this thread's private form"; return; }
    try {
        auto *form = (EForm *)GetWindowLongPtrW(window, GWLP_USERDATA);
        require(form && form->window == window, "Production private form binding unavailable");
        require((WNDPROC)GetClassLongPtrW(window, GCLP_WNDPROC) == EForm::proc, "Unexpected own form procedure");
        require(!IsWindowVisible(window), "Private form must remain hidden");
        callbackRan = true; formAction(*form);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDOK, 0), (LPARAM)form->ctl(IDOK));
        currentForm=nullptr;
    } catch (const std::exception &error) {
        callbackError = error.what(); SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
    }
}
LRESULT CALLBACK onCreate(int code, WPARAM value, LPARAM info) {
    if (code == HCBT_CREATEWND && qaOwnForm((HWND)value)) {
        if (currentForm) callbackError = "Unexpected second private form";
        else { currentForm = (HWND)value; if (!SetTimer(currentForm, formTimer, 1, fillForm)) callbackError = "Private form timer failed"; }
    }
    return CallNextHookEx(creationHook, code, value, info);
}
template<class Action, class Invoke> bool runForm(Action action, Invoke invoke) {
    currentForm = nullptr; callbackRan = false; callbackError.clear(); formAction = action; qaPrivateForm = true;
    creationHook = SetWindowsHookExW(WH_CBT, onCreate, nullptr, GetCurrentThreadId());
    require(creationHook != nullptr, "Own thread form hook unavailable");
    bool accepted = false;
    try { accepted = invoke(); } catch (...) { UnhookWindowsHookEx(creationHook); creationHook = nullptr; qaPrivateForm = false; throw; }
    UnhookWindowsHookEx(creationHook); creationHook = nullptr; qaPrivateForm = false; formAction = {};
    require(callbackError.empty(), callbackError.c_str()); require(callbackRan, "Private form callback did not execute");
    return accepted;
}
void forms() {
    EverWindow editor; editor.dpi = 96;
    ever::Rule rule; bool remember = false;
    bool ok = runForm([](EForm &form) {
        choose(form, 103, L"数字递增");
        editCombo(form, 103, L"普通文字😀");
        form.set(102, L"-1"); tick(form, 113, true);
        check(form.get(103) == L"普通文字😀" && !fieldVisible(form, 108) && (GetWindowLongPtrW(form.ctl(103), GWL_STYLE) & 3) == CBS_DROPDOWN,
              "ordinary-source-edit-clears-old-choice-and-updates-fields");
    }, [&] { return editor.ruleForm(rule, remember); });
    check(ok && rule.source == ever::AddSource::Text && rule.text == L"普通文字😀" && rule.index == -1 && remember,
          "ordinary-source-and-memory-return-from-production-rule-form");

    rule = ever::Rule{}; remember = false;
    ok = runForm([](EForm &form) {
        choose(form, 103, L"数字递增"); form.set(108, L"001.5"); form.set(111, L"-"); form.set(102, L"1"); tick(form, 110, true);
        check(fieldVisible(form, 108) && fieldVisible(form, 111) && !fieldVisible(form, 109) && !fieldVisible(form, 104),
              "sequence-choice-shows-start-separator-hides-pinyin-delete");
    }, [&] { return editor.ruleForm(rule, remember); });
    check(ok && rule.source == ever::AddSource::Sequence && rule.sequenceStart == L"001.5" && rule.separator == L"-" && rule.index == 1 && rule.replaceEntireName && !remember,
          "sequence-values-return-from-production-rule-form");

    rule = ever::Rule{};
    ok = runForm([](EForm &form) {
        choose(form, 103, L"中文转拼音首字母"); form.set(109, L"0");
        require(fieldVisible(form, 109) && !fieldVisible(form, 108), "Pinyin dynamic field mismatch");
    }, [&] { return editor.ruleForm(rule, remember); });
    check(ok && rule.source == ever::AddSource::PinyinInitials && rule.pinyinCount == 0,
          "pinyin-zero-is-accepted-and-returned");

    rule = ever::Rule{};
    ok = runForm([](EForm &form) {
        choose(form, 103, L"中文转数字"); form.set(106, L"前😀.*"); form.set(107, L"后🚀$"); tick(form, 112, true);
        require(fieldVisible(form, 106) && fieldVisible(form, 107) && fieldVisible(form, 112) && !fieldVisible(form, 109), "Number anchors dynamic field mismatch");
    }, [&] { return editor.ruleForm(rule, remember); });
    check(ok && rule.source == ever::AddSource::ChineseNumbers && rule.frontAnchor == L"前😀.*" && rule.backAnchor == L"后🚀$" && rule.replaceOriginalNumbers,
          "front-back-anchors-and-number-replacement-return");

    rule = ever::Rule{};
    ok = runForm([](EForm &form) {
        choose(form, 101, L"删减"); SetWindowTextW(form.ctl(101), L"自由输入非法操作");
        form.set(102, L"3"); form.set(104, L"2");
        check((GetWindowLongPtrW(form.ctl(101), GWL_STYLE) & 3) == CBS_DROPDOWNLIST && form.get(101) == L"删减" && fieldVisible(form, 104) && !fieldVisible(form, 103),
              "strict-operation-dropdown-rejects-free-text-and-shows-delete");
    }, [&] { return editor.ruleForm(rule, remember); });
    check(ok && rule.operation == ever::Operation::Delete && rule.index == 3 && rule.count == 2,
          "delete-operation-values-return");

    rule = ever::Rule{}; remember = false;
    ok = runForm([](EForm &form) {
        require(fieldVisible(form, 104) && fieldVisible(form, 107) && fieldVisible(form, 106), "Initial regex fields mismatch");
        tick(form, 105, false);
        check(!fieldVisible(form, 104) && !fieldVisible(form, 107) && fieldVisible(form, 106), "regex-disabled-hides-only-ignorecase-multiline");
        tick(form, 105, true); tick(form, 102, true); tick(form, 103, false); tick(form, 104, false); tick(form, 106, true); tick(form, 107, false); tick(form, 108, true);
        form.set(101, L"旧😀|新🚀\r\n(?<n>\\d+)|${n}");
        check(fieldVisible(form, 104) && fieldVisible(form, 107) && fieldVisible(form, 106), "regex-enabled-restores-dynamic-fields");
    }, [&] { return editor.replacementForm(rule, remember); });
    check(ok && rule.mode == ever::Mode::FindReplace && rule.replacements == L"旧😀|新🚀\r\n(?<n>\\d+)|${n}" && rule.regex && rule.escapeFind && !rule.escapeReplacement && !rule.ignoreCase && rule.singleline && !rule.multiline && remember,
          "replacement-fields-flags-and-memory-return");
    rule=ever::Rule{};rule.sequenceWidth=3;rule.sequenceStart=L"1";
    ok=runForm([](EForm&form){form.set(101,L"xxxx-{num}-{taken:yyyyMMdd}");form.set(102,L"20");form.set(103,L"4");form.set(104,L"2");tick(form,106,true);},[&]{return editor.namingForm(rule,remember);});
    check(ok&&rule.mode==ever::Mode::NamingTemplate&&rule.sequenceStart==L"20"&&rule.sequenceWidth==4&&rule.sequenceStep==2&&remember,"custom-number-and-metadata-form-fields-return");
    Scintilla_RegisterClasses(GetModuleHandleW(nullptr));
    HWND host=CreateWindowExW(0,L"STATIC",L"private-editor-test",WS_OVERLAPPED,0,0,700,400,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    editor.window=host;editor.editor=CreateWindowExW(0,L"Scintilla",L"",WS_CHILD,0,0,600,300,host,nullptr,GetModuleHandleW(nullptr),nullptr);
    require(editor.editor!=nullptr,"private production editor unavailable");
    const std::wstring original=L"字段😀-a.jpg\n字段😀-b.jpg\n字段😀-c.jpg";
    editor.setText(original);editor.sci(SCI_SETSEL,0,eUtf8(L"字段😀").size());
    runForm([](EForm&form){check(form.get(101)==L"字段😀"&&!form.check(105),"selected-word-prefills-find-but-default-scope-is-all-names");form.set(102,L"新");},[&]{editor.findReplace();return true;});
    check(editor.text()==L"新-a.jpg\n新-b.jpg\n新-c.jpg","selected-word-replacement-updates-all-three-rows");
    editor.sci(SCI_UNDO);check(editor.text()==original,"batch-replacement-undo-restores-all-rows");
    editor.sci(SCI_SETSEL,0,eUtf8(L"字段😀").size());
    runForm([](EForm&form){form.set(102,L"仅首行");tick(form,105,true);},[&]{editor.findReplace();return true;});
    check(editor.text()==L"仅首行-a.jpg\n字段😀-b.jpg\n字段😀-c.jpg","explicit-selection-scope-is-still-supported");
    editor.items.clear();for(const auto&name:eLines(original)){Entry entry;entry.name=name;entry.path=eTestHome+name;editor.items.push_back(entry);}
    editor.setText(original);ever::Rule numbering;numbering.mode=ever::Mode::NamingTemplate;numbering.sequenceStart=L"1";numbering.sequenceWidth=3;numbering.namingPattern=L"xxxx-{num}";
    check(editor.apply(numbering)&&editor.text()==L"xxxx-001.jpg\nxxxx-002.jpg\nxxxx-003.jpg","number-template-batch-preview-retains-extensions");
    editor.toggle(true);numbering.namingPattern=L"图-{num}";numbering.sequenceStart=L"10";
    check(editor.apply(numbering)&&editor.text()==L"图-010\n图-011\n图-012","hidden-extension-mode-uses-same-template");
    editor.toggle(false);check(editor.text()==L"图-010.jpg\n图-011.jpg\n图-012.jpg","showing-extension-after-numbering-does-not-duplicate-it");
    check(qaWarnings==0,"all-valid-forms-and-editor-operations-avoid-warnings");
    const std::wstring beforeFailure=editor.text();numbering.namingPattern=L"{taken}-{num}";
    check(!editor.apply(numbering)&&editor.text()==beforeFailure&&qaWarnings==1,"missing-EXIF-keeps-the-whole-preview-unchanged");qaWarnings=0;
    runForm([](EForm&form){form.set(101,L"\n");form.set(102,L"");},[&]{editor.findReplace();return true;});
    check(editor.text()==beforeFailure&&qaWarnings==1,"replacement-that-removes-rows-is-rejected-atomically");qaWarnings=0;
#ifdef LITELAUNCH_BATCH_PLUGIN
    const wchar_t*advancedTypes[]={L"Insert · 插入",L"Delete · 删除 / 清空",L"Remove · 移除",L"Replace · 替换",L"Rearrange · 重排",L"Extension · 扩展名",L"Strip · 剥离字符",L"Case · 大小写",L"Serialize · 序列化",L"CleanUp · 清理",L"Translit · 转写",L"ReformatDate · 日期重排",L"PascalScript · Pascal 脚本",L"RegEx · 正则表达式",L"UserInput · 名称列表",L"Randomize · 随机序列",L"Mapping · CSV 映射",L"Padding · 填充",L"Metadata · 元数据模板"};
    for(const auto*label:advancedTypes){batch::Step advanced;advanced.kind=batch::Kind::Advanced;int panels=0;
        bool accepted=runForm([&](EForm&form){if(panels++==0){check(form.fields[0].choices.size()==19,"all-18-rule-types-plus-metadata-template-are-selectable");choose(form,101,label);}else{check(form.fields.size()>1,"advanced-rule-has-parameter-controls");if(std::wstring(label).find(L"Serialize")==0){form.set(101,L"10");form.set(102,L"-2");}for(const auto&field:form.fields){RECT labelBox;GetWindowRect(form.ctl(field.id+1000),&labelBox);HDC dc=GetDC(form.window);SelectObject(dc,form.font);RECT need={0,0,labelBox.right-labelBox.left,0};DrawTextW(dc,field.label.c_str(),-1,&need,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);ReleaseDC(form.window,dc);require(labelBox.bottom-labelBox.top>=need.bottom,"Advanced parameter label was clipped");}}},[&]{return editor.batchForm(advanced);});
        check(accepted&&panels==2&&advanced.kind==batch::Kind::Advanced&&advanced.options.count(L"type"),"advanced-rule-selection-and-parameters-round-trip");
        if(std::wstring(label).find(L"Serialize")==0)check(advanced.options[L"start"]==L"10"&&advanced.options[L"step"]==L"-2","serialize-form-preserves-negative-step");
    }
    batch::Step pipelineStep;
    check(runForm([](EForm&form){choose(form,101,L"正则替换");form.set(106,L"A|B");form.set(107,L"C");},[&]{return editor.batchForm(pipelineStep);}),"pipeline-regex-form-accepts-alternation");
    check(pipelineStep.rule.regex&&pipelineStep.rule.replacements==L"|=\u241e\nA|B\u241eC","pipeline-regex-form-preserves-alternation-with-safe-delimiter");
    check(runForm([](EForm&form){choose(form,101,L"编号 / 元数据模板");form.set(108,L"{name}-{num}");form.set(109,L"7");form.set(110,L"2");form.set(111,L"4");require(fieldVisible(form,108)&&!fieldVisible(form,106),"Pipeline fields did not switch");},[&]{return editor.batchForm(pipelineStep);}),"pipeline-number-template-form-binds-and-switches-fields");
    check(pipelineStep.rule.sequenceStart==L"7"&&pipelineStep.rule.sequenceStep==2&&pipelineStep.rule.sequenceWidth==4,"pipeline-number-parameters-read-back-from-real-controls");
    check(runForm([](EForm&form){choose(form,101,L"修改扩展名");form.set(102,L"jpg");},[&]{return editor.batchForm(pipelineStep);})&&pipelineStep.kind==batch::Kind::Extension&&pipelineStep.text==L"jpg","pipeline-extension-form-binds-specific-operation");
    check(runForm([](EForm&form){choose(form,101,L"删除字符");choose(form,113,L"清空名称");check(!fieldVisible(form,104)&&!fieldVisible(form,105),"clear-mode-hides-position-and-count");},[&]{return editor.batchForm(pipelineStep);})&&pipelineStep.kind==batch::Kind::Clear,"delete-form-offers-working-clear-operation");
    editor.batchSimple=true;editor.hidden=false;editor.rebuild();
    runForm([](EForm&form){check(form.get(102)==L"1"&&form.get(103)==L"3"&&!fieldVisible(form,106),"simple-numbering-starts-with-valid-defaults-and-no-unused-memory-option");},[&]{editor.batchQuick(EverWindow::BR_QNUMBER);return true;});
    check(editor.text()==L"字段😀-a-001.jpg\n字段😀-b-002.jpg\n字段😀-c-003.jpg","simple-number-button-runs-production-form-and-updates-current-preview");
    for(int scale:{96,120,144,192}){
        EForm measured;measured.fields={{101,L"位置（1表示开头，-1表示末尾）",L"1"},{102,L"保留扩展名，不参与本条规则",L"1",1},{103,L"字符一行一对\n查找内容|替换内容",L"",2}};
        runForm([&](EForm&form){
            HDC dc=GetDC(form.window);SelectObject(dc,form.font);bool fits=true;RECT previous={};
            for(const auto&field:form.fields){RECT box,input;GetWindowRect(form.ctl(field.id+1000),&box);GetWindowRect(form.ctl(field.id),&input);RECT need={0,0,box.right-box.left,0};DrawTextW(dc,field.label.c_str(),-1,&need,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);if(box.bottom-box.top<need.bottom||(previous.bottom&&box.top<previous.bottom))fits=false;previous.bottom=std::max(box.bottom,input.bottom);}
            ReleaseDC(form.window,dc);check(fits,"wrapped-labels-fit-and-do-not-overlap-next-row-at-tested-scale");
        },[&]{return measured.run(nullptr,scale,L"参数换行回归");});
    }
    EForm tall;for(int i=0;i<24;i++)tall.fields.push_back({101+i,L"长表单滚动检查",L"内容"});
    runForm([](EForm&form){RECT box,work;GetWindowRect(form.window,&box);SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);check(box.top>=work.top&&box.bottom<=work.bottom&&form.scrollMax>0,"long-form-fits-work-area-and-exposes-scrollbar");SendMessageW(form.window,WM_VSCROLL,SB_PAGEDOWN,0);check(form.scrollY>0,"form-page-scroll-moves-content");},[&]{return tall.run(nullptr,192,L"长表单回归");});
#endif
    DestroyWindow(host);editor.window=nullptr;editor.editor=nullptr;
    check(qaWarnings == 0, "form-bindings-complete-without-validation-warning");
}
}
int wmain(int argc, wchar_t **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        require(argc == 3, "Usage: ever_form_test <unique-owned-root> <regex-worker>");ever::regexTestExecutable(argv[2]);
        const std::wstring root = fullPath(argv[1]);
        require(fileName(root).compare(0, 16, L"LL_EVER_FORM_QA_") == 0, "Private form QA root name required");
        require(CreateDirectoryW(root.c_str(), nullptr) != FALSE, "Private form QA root must be new");
        const std::wstring state = root + L"\\state"; require(CreateDirectoryW(state.c_str(), nullptr) != FALSE, "Private form state directory unavailable");
        eTestHome = state + L"\\";
        HRESULT ole = OleInitialize(nullptr); require(SUCCEEDED(ole), "Private form COM initialization failed");
        INITCOMMONCONTROLSEX common{sizeof(common), ICC_WIN95_CLASSES}; InitCommonControlsEx(&common);
        forms(); ever::regexShutdown();OleUninitialize();
        std::printf("RESULT passed=%d failed=%d\n", passed, failed); return failed ? 1 : 0;
    } catch (const std::exception &error) {
        if (creationHook) UnhookWindowsHookEx(creationHook);
        std::printf("FATAL %s Win32=%lu\n", error.what(), GetLastError()); return 2;
    }
}
