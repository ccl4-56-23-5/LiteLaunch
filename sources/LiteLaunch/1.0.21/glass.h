#pragma once
#include <windows.h>
enum { GLASS_HEADER_HEIGHT=56, GLASS_GRID_TOP=70, GLASS_ROW_PITCH=104, GLASS_FOOTER_HEIGHT=38,
       GLASS_FUNCTION_SLOTS=4, GLASS_SLOT_CAPACITY=132 };
#ifdef __cplusplus
extern "C" {
#endif
void glassMaskPixels(void *pixels,int width,int height,int dpi);
/* count includes the fixed function row; offset scrolls application rows only. */
void glassRows(int count,int offset);
void glassInit(void);
void glassShutdown(void);
void glassInteraction(float x,float y,int from,int to);
void glassReorder(int from,int to);
void glassResetSelection(void);
void glassTheme(int theme);
COLORREF glassAccentColor(void);
HBITMAP glassThemeSwatch(int theme,int size);
BOOL glassNeedsPaint(void);
void glassCapture(int x,int y,int w,int h);
void *glassLoadIcon(const WCHAR *path,const WCHAR *name,const WCHAR *target,int dpi);
void glassFreeIcon(void *icon);
BOOL glassExportIcon(void *icon,const WCHAR *path);
void glassPaint(HDC dc,int width,int height,int dpi,int hover,int selected,BOOL pinned,const WCHAR **names,void **icons,const BOOL *occupied);
#ifdef __cplusplus
}
#endif
