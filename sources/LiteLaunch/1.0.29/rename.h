#pragma once
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Shows a modal native batch rename window. No rename occurs before Execute. */
void renameShow(HWND owner, HWND sourceWindow, int dpi);
void renameShowCommand(HWND owner, HWND sourceWindow, int dpi, LPCWSTR command);
HWND renameActiveWindow(void);
#ifdef __cplusplus
}
#endif
