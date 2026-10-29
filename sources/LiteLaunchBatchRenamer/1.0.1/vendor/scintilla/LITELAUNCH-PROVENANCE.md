# Scintilla 5.6.6 for LiteLaunch 1.0.13

Official source archive: https://www.scintilla.org/scintilla566.zip

Downloaded on 2026-09-27. Archive SHA-256:

`A0C0CDF1CF226DC6252020CE9A87A939A8B615E65269DB40AC9123B28FA20A9D`

The archive's complete `scintilla` tree is retained, including `License.txt`, public headers and official Windows makefiles. Official files are unchanged. `LiteLaunchEditor.h`, `LiteLaunchFixture.cxx`, and this provenance file are LiteLaunch integration additions.

The static build follows `win32/makefile`: 33 core C++ sources plus six Windows sources, excluding `ScintillaDLL.cxx` and DLL resources. All require C++17. The reproducible PowerShell script is `../../qa/build-scintilla.ps1`; it uses Zig 0.15.2 and puts objects, static archive, manifest and verification executable under workspace `temp`, outside versioned source.

Link `libscintilla.a` and the Windows libraries `gdi32`, `user32`, `imm32`, `ole32`, `uuid`, `oleaut32`, `advapi32`. Call `Scintilla_RegisterClasses` once with the application instance, then create a `Scintilla` child window and configure it with `LiteLaunchEditor::Configure`. Static linking requires no Scintilla DLL, Quicker, compiler or downloaded runtime at application launch. Direct2D/DirectWrite are loaded from Windows by official Scintilla code.

Keep `License.txt` in the source and include its copyright/permission notice in distributed supporting documentation.

Official API/build reference: https://www.scintilla.org/ScintillaDoc.html

The fixture verifies native Windows Alt rectangle, Ctrl multiple carets, simultaneous typing and undo using only a private in-process editor. It does not rename files, alter startup settings, set the system clipboard, or send global mouse/keyboard input.
