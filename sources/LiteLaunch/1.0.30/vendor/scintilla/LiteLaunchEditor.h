// LiteLaunch integration additions; official Scintilla sources are unchanged.
#pragma once
#include <windows.h>
#include "include/Scintilla.h"

namespace LiteLaunchEditor {
inline LRESULT Send(HWND editor, UINT message, WPARAM wParam = 0, LPARAM lParam = 0) {
    return SendMessageW(editor, message, wParam, lParam);
}

// Call Scintilla_RegisterClasses(GetModuleHandleW(nullptr)) once before creating
// any editor. Call Scintilla_ReleaseResources only after all editors are destroyed.
// Text, selection positions, and lengths use UTF-8 bytes, not UTF-16 code units.
inline void Configure(HWND editor, int fontPoints = 12) {
    Send(editor, SCI_SETCODEPAGE, SC_CP_UTF8);
    Send(editor, SCI_SETEOLMODE, SC_EOL_LF);
    Send(editor, SCI_SETWRAPMODE, SC_WRAP_NONE);
    Send(editor, SCI_SETMULTIPLESELECTION, TRUE);
    Send(editor, SCI_SETADDITIONALSELECTIONTYPING, TRUE);
    Send(editor, SCI_SETADDITIONALCARETSVISIBLE, TRUE);
    Send(editor, SCI_SETADDITIONALCARETSBLINK, TRUE);
    Send(editor, SCI_SETMULTIPASTE, SC_MULTIPASTE_EACH);
    Send(editor, SCI_SETUNDOSELECTIONHISTORY, SC_UNDO_SELECTION_HISTORY_ENABLED);
    Send(editor, SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION | SCVS_USERACCESSIBLE);
    Send(editor, SCI_SETMOUSESELECTIONRECTANGULARSWITCH, TRUE);
    // Windows native mouse handling always uses Alt for rectangles. The
    // SETRECTANGULARSELECTIONMODIFIER message is only implemented on GTK/Qt.
    Send(editor, SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>("Consolas"));
    Send(editor, SCI_STYLESETSIZE, STYLE_DEFAULT, fontPoints);
    Send(editor, SCI_STYLECLEARALL);
    Send(editor, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    const int digitWidth = static_cast<int>(Send(editor, SCI_TEXTWIDTH, STYLE_LINENUMBER, reinterpret_cast<LPARAM>("99999")));
    Send(editor, SCI_SETMARGINWIDTHN, 0, digitWidth + 8);
    Send(editor, SCI_SETMARGINWIDTHN, 1, 0);
    Send(editor, SCI_SETMARGINWIDTHN, 2, 0);
    Send(editor, SCI_SETSCROLLWIDTH, 1);
    Send(editor, SCI_SETSCROLLWIDTHTRACKING, TRUE);
    Send(editor, SCI_SETTABWIDTH, 4);
    Send(editor, SCI_SETUSETABS, FALSE);
}
} // namespace LiteLaunchEditor
