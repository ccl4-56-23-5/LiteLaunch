// Isolated real Scintilla Windows-control regression; never opens user files.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include "LiteLaunchEditor.h"

using LiteLaunchEditor::Send;
namespace {
int passed = 0, failed = 0;
void check(bool ok, const char *name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    ok ? ++passed : ++failed;
}
std::string text(HWND editor) {
    const auto length = Send(editor, SCI_GETLENGTH);
    std::vector<char> buffer(static_cast<size_t>(length) + 1);
    Send(editor, SCI_GETTEXT, buffer.size(), reinterpret_cast<LPARAM>(buffer.data()));
    return std::string(buffer.data(), static_cast<size_t>(length));
}
void setText(HWND editor, const char *value) {
    Send(editor, SCI_SETSELECTIONMODE, SC_SEL_STREAM);
    Send(editor, SCI_CLEARSELECTIONS);
    Send(editor, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(value));
    Send(editor, SCI_EMPTYUNDOBUFFER);
}
POINT pointAt(HWND editor, LRESULT position) {
    return POINT{static_cast<LONG>(Send(editor, SCI_POINTXFROMPOSITION, 0, position)),
        static_cast<LONG>(Send(editor, SCI_POINTYFROMPOSITION, 0, position) + Send(editor, SCI_TEXTHEIGHT, 0) / 2)};
}
void mouse(HWND editor, UINT message, POINT point, WPARAM buttons) {
    SendMessageW(editor, message, buttons, MAKELPARAM(point.x, point.y));
}
struct ThreadKeyboard {
    BYTE saved[256]{};
    bool valid = GetKeyboardState(saved) != FALSE;
    void modifier(int key = 0) {
        BYTE state[256]{};
        if (key) state[key] = 0x80;
        SetKeyboardState(state);
    }
    ~ThreadKeyboard() { if (valid) SetKeyboardState(saved); }
};
void click(HWND editor, POINT point, WPARAM modifiers) {
    mouse(editor, WM_LBUTTONDOWN, point, modifiers | MK_LBUTTON);
    mouse(editor, WM_LBUTTONUP, point, modifiers);
}
void rectangle(HWND editor, POINT start, POINT end, ThreadKeyboard &keys) {
    keys.modifier(VK_MENU);
    mouse(editor, WM_LBUTTONDOWN, start, MK_LBUTTON);
    // Scintilla throttles successive drag updates by its auto-scroll counter.
    // A private message burst models the intermediate events of a real drag.
    for (int i = 0; i < 16; ++i) {
        mouse(editor, WM_MOUSEMOVE, POINT{end.x + (i % 2), end.y}, MK_LBUTTON);
    }
    mouse(editor, WM_MOUSEMOVE, end, MK_LBUTTON);
    mouse(editor, WM_LBUTTONUP, end, 0);
    keys.modifier();
}
void type(HWND editor, const wchar_t *value) {
    while (*value) SendMessageW(editor, WM_CHAR, static_cast<WPARAM>(*value++), 1);
}
void dump(HWND editor, const char *label) {
    std::printf("DIAG %s text=[%s] selections=%lld mode=%lld\n", label, text(editor).c_str(),
        static_cast<long long>(Send(editor, SCI_GETSELECTIONS)), static_cast<long long>(Send(editor, SCI_GETSELECTIONMODE)));
    for (int i = 0; i < Send(editor, SCI_GETSELECTIONS); ++i) {
        std::printf("  selection%d caret=%lld anchor=%lld virtual=%lld/%lld\n", i,
            static_cast<long long>(Send(editor, SCI_GETSELECTIONNCARET, i)), static_cast<long long>(Send(editor, SCI_GETSELECTIONNANCHOR, i)),
            static_cast<long long>(Send(editor, SCI_GETSELECTIONNCARETVIRTUALSPACE, i)), static_cast<long long>(Send(editor, SCI_GETSELECTIONNANCHORVIRTUALSPACE, i)));
    }
}
}

int main() {
    SetProcessDPIAware();
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    check(Scintilla_RegisterClasses(instance) != 0, "register-static-Scintilla-class");
    HWND owner = CreateWindowExW(0, L"STATIC", L"LiteLaunch isolated Scintilla fixture", WS_OVERLAPPEDWINDOW,
        0, 0, 900, 600, nullptr, nullptr, instance, nullptr);
    HWND editor = CreateWindowExW(0, L"Scintilla", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 820, 520, owner, nullptr, instance, nullptr);
    check(owner != nullptr && editor != nullptr, "create-private-hidden-editor");
    if (!owner || !editor) {
        if (owner) DestroyWindow(owner);
        Scintilla_ReleaseResources();
        return 2;
    }
    LiteLaunchEditor::Configure(editor);
    check(Send(editor, SCI_GETCODEPAGE) == SC_CP_UTF8, "UTF8-codepage");
    check(Send(editor, SCI_GETEOLMODE) == SC_EOL_LF, "LF-line-endings");
    check(Send(editor, SCI_GETWRAPMODE) == SC_WRAP_NONE, "unwrapped-editor");
    check(Send(editor, SCI_GETMULTIPLESELECTION) == TRUE, "multiple-selections-enabled");
    check(Send(editor, SCI_GETADDITIONALSELECTIONTYPING) == TRUE, "simultaneous-selection-typing-enabled");
    check(Send(editor, SCI_GETADDITIONALCARETSVISIBLE) == TRUE, "additional-carets-visible");
    check(Send(editor, SCI_GETMULTIPASTE) == SC_MULTIPASTE_EACH, "paste-to-each-selection-configured");
    check(Send(editor, SCI_GETVIRTUALSPACEOPTIONS) == (SCVS_RECTANGULARSELECTION | SCVS_USERACCESSIBLE), "rectangle-virtual-space-enabled");
    check(Send(editor, SCI_GETMOUSESELECTIONRECTANGULARSWITCH) == TRUE, "rectangle-switch-enabled");
    check(Send(editor, SCI_GETUNDOSELECTIONHISTORY) == SC_UNDO_SELECTION_HISTORY_ENABLED, "selection-history-enabled");
    check(Send(editor, SCI_GETMARGINWIDTHN, 0) > 0, "line-number-margin");
    check(Send(editor, SCI_GETDIRECTFUNCTION) != 0, "direct-function-available");
    setText(editor, u8"甲.txt\n乙文档.pdf\n中文😀.png");
    check(text(editor) == u8"甲.txt\n乙文档.pdf\n中文😀.png", "Chinese-and-nonBMP-roundtrip");
    check(Send(editor, SCI_GETLINECOUNT) == 3, "three-logical-lines");
    {
        ThreadKeyboard keys;
        keys.modifier();
        check(keys.valid, "thread-keyboard-state-saved");
        setText(editor, "ABCDE\nABCDE\nABCDE");
        rectangle(editor, pointAt(editor, 1), pointAt(editor, Send(editor, SCI_POSITIONFROMLINE, 2) + 3), keys);
        check(Send(editor, SCI_SELECTIONISRECTANGLE) != 0, "native-Alt-drag-is-rectangle");
        check(Send(editor, SCI_GETSELECTIONS) == 3, "native-Alt-drag-selects-three-lines");
        bool correctRanges = Send(editor, SCI_GETSELECTIONS) == 3;
        for (int i = 0; i < Send(editor, SCI_GETSELECTIONS); ++i) {
            const LRESULT caret = Send(editor, SCI_GETSELECTIONNCARET, i), anchor = Send(editor, SCI_GETSELECTIONNANCHOR, i);
            const LRESULT line = Send(editor, SCI_LINEFROMPOSITION, std::min(caret, anchor));
            const LRESULT lineStart = Send(editor, SCI_POSITIONFROMLINE, line);
            correctRanges = correctRanges && std::min(caret, anchor) - lineStart == 1 && std::max(caret, anchor) - lineStart == 3;
        }
        check(correctRanges, "rectangle-byte-ranges-exact");
        type(editor, L"X");
        check(text(editor) == "AXDE\nAXDE\nAXDE", "native-typing-replaces-all-rectangle-ranges");
        check(Send(editor, SCI_CANUNDO) != 0, "rectangle-change-has-undo");
        Send(editor, SCI_UNDO);
        check(text(editor) == "ABCDE\nABCDE\nABCDE", "undo-restores-all-rectangle-lines");
        Send(editor, SCI_REDO);
        check(text(editor) == "AXDE\nAXDE\nAXDE", "redo-restores-all-rectangle-lines");

        setText(editor, u8"甲\n乙\n丙");
        const LRESULT firstEnd = Send(editor, SCI_GETLINEENDPOSITION, 0);
        Send(editor, SCI_SETSELECTION, firstEnd, firstEnd);
        keys.modifier(VK_CONTROL);
        click(editor, pointAt(editor, Send(editor, SCI_GETLINEENDPOSITION, 1)), MK_CONTROL);
        click(editor, pointAt(editor, Send(editor, SCI_GETLINEENDPOSITION, 2)), MK_CONTROL);
        keys.modifier();
        check(Send(editor, SCI_GETSELECTIONS) == 3, "native-Ctrl-click-creates-three-carets");
        type(editor, L"名");
        check(text(editor) == u8"甲名\n乙名\n丙名", "native-Chinese-typing-at-all-carets");
        Send(editor, SCI_UNDO);
        check(text(editor) == u8"甲\n乙\n丙", "undo-restores-all-Chinese-carets");
        Send(editor, SCI_REDO);
        check(text(editor) == u8"甲名\n乙名\n丙名", "redo-restores-all-Chinese-carets");
        check(Send(editor, SCI_GETSELECTIONS) == 3, "redo-restores-multiple-carets");
        type(editor, L"\xD83D\xDE80");
        if (text(editor) != u8"甲名🚀\n乙名🚀\n丙名🚀") dump(editor, "surrogate");
        check(text(editor) == u8"甲名🚀\n乙名🚀\n丙名🚀", "native-surrogate-typing-at-all-carets");

        setText(editor, "A\nABC\nABCDE");
        const POINT line0End = pointAt(editor, 1);
        const POINT line2Col4 = pointAt(editor, Send(editor, SCI_POSITIONFROMLINE, 2) + 4);
        rectangle(editor, POINT{line2Col4.x, line0End.y}, line2Col4, keys);
        if (Send(editor, SCI_SELECTIONISRECTANGLE) == 0 || Send(editor, SCI_GETSELECTIONS) != 3) dump(editor, "short-rectangle");
        check(Send(editor, SCI_SELECTIONISRECTANGLE) != 0 && Send(editor, SCI_GETSELECTIONS) == 3, "native-rectangle-through-short-lines");
        type(editor, L"Z");
        if (text(editor) != "A   Z\nABC Z\nABCDZE") dump(editor, "short-rectangle-typing");
        check(text(editor) == "A   Z\nABC Z\nABCDZE", "rectangle-typing-pads-short-lines");
    }
    setText(editor, "read only");
    Send(editor, SCI_SETREADONLY, TRUE);
    type(editor, L"X");
    check(text(editor) == "read only", "read-only-pane-blocks-native-typing");
    Send(editor, SCI_SETREADONLY, FALSE);
    Send(editor, SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE);
    check(Send(editor, SCI_GETTECHNOLOGY) == SC_TECHNOLOGY_DIRECTWRITE, "Windows-DirectWrite-available");
    HWND second = CreateWindowExW(0, L"Scintilla", L"", WS_CHILD, 0, 0, 200, 100, owner, nullptr, instance, nullptr);
    check(second != nullptr, "second-editor-shares-registered-class");
    if (second) {
        LiteLaunchEditor::Configure(second);
        setText(second, "independent document");
        check(text(editor) == "read only" && text(second) == "independent document", "two-editor-documents-independent");
        DestroyWindow(second);
    }
    DestroyWindow(editor);
    DestroyWindow(owner);
    check(Scintilla_ReleaseResources() != 0, "release-after-controls-destroyed");
    check(Scintilla_RegisterClasses(instance) != 0, "re-register-after-release");
    check(Scintilla_ReleaseResources() != 0, "second-resource-release");
    std::printf("RESULT passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
