/* ui_theme.h -- LiteLaunch 1.0.16 shared visual design tokens.
 *
 * One source of truth for colour, radius and metric tokens used by the panel
 * renderer (glass.cpp), the dialog system (picker_style.h, main.c) and the
 * EVER rename windows (ever_window.inc).  The header is deliberately plain C:
 * it is included from C11 and C++17 translation units alike, and it never
 * mentions GDI+ so it stays usable from picker_style.h.
 *
 * Included by both C and C++, every symbol is either a macro or a `static`
 * object/function, so no linkage is exported and multiple inclusion across
 * translation units is safe.
 *
 * NOTE: theme indices are persisted in LiteLaunch.ini (Panel/Theme).  The ten
 * slots keep their original order and names; only the RGB values were retuned
 * for contrast on light surfaces.  Never reorder or remove a slot.
 */
#ifndef LITELAUNCH_UI_THEME_H
#define LITELAUNCH_UI_THEME_H

#include <windows.h>

/* Header-local objects must not trip -Wunused-const-variable in C units that
   only need part of the token set. */
#if defined(__GNUC__) || defined(__clang__)
#define UI_UNUSED __attribute__((unused))
#else
#define UI_UNUSED
#endif

/* ---------------------------------------------------------------- neutrals */
#define UI_INK          RGB( 31,  42,  55)  /* primary text            #1F2A37 */
#define UI_INK_SOFT     RGB( 71,  85, 105)  /* secondary text          #475569 */
#define UI_MUTED        RGB(100, 116, 139)  /* captions, hints         #64748B */
#define UI_FAINT        RGB(148, 163, 184)  /* disabled text           #94A3B8 */
#define UI_LINE         RGB(226, 232, 240)  /* hairline borders        #E2E8F0 */
#define UI_LINE_SOFT    RGB(238, 242, 247)  /* inner dividers          #EEF2F7 */
#define UI_SURFACE      RGB(255, 255, 255)  /* cards, inputs           #FFFFFF */
#define UI_SURFACE_ALT  RGB(247, 249, 252)  /* recessed surfaces       #F7F9FC */
#define UI_CANVAS       RGB(244, 246, 250)  /* dialog background       #F4F6FA */
#define UI_ACCENT_WARM  RGB(178,  65,  79)  /* destructive hover       #B2414F */
#define UI_DANGER_SOFT  RGB(255, 243, 244)  /* destructive hover fill  #FFF3F4 */
#define UI_DANGER_LINE  RGB(222,  99, 111)  /* destructive border      #DE636F */

/* ------------------------------------------------------------------- radii */
#define UI_R_PANEL      30   /* layered panel window mask (must match glass.h)  */
#define UI_R_CARD       16   /* application tiles                               */
#define UI_R_PLATE      12   /* icon plates, glyph plates                       */
#define UI_R_BUTTON     11   /* dialog buttons                                  */
#define UI_R_FIELD      10   /* text fields, list boxes                         */
#define UI_R_PILL        9   /* compact chips inside the panel                  */

/* ---------------------------------------------------------------- metrics */
#define UI_GAP          8    /* base spacing unit                               */

/* ------------------------------------------------------- accent theme table */
#define UI_THEME_COUNT  10

/* Index order is part of the on-disk configuration contract. */
static const unsigned char uiThemeRgb[UI_THEME_COUNT][3] UI_UNUSED = {
    { 122, 106, 216 },  /* 0 丁香紫   lilac    */
    {  16, 181, 165 },  /* 1 蒂芙尼蓝 tiffany  */
    { 242, 107,  58 },  /* 2 熔岩橙   lava     */
    {  61, 111, 227 },  /* 3 龙胆蓝   gentian  */
    { 225,  75, 133 },  /* 4 玫红     rose     */
    {  30, 158, 100 },  /* 5 松叶绿   pine     */
    { 217, 154,  22 },  /* 6 琥珀金   amber    */
    {  46, 151, 201 },  /* 7 湖水蓝   lake     */
    { 150, 119,  92 },  /* 8 暖咖棕   mocha    */
    {  90, 107, 128 }   /* 9 石墨灰   graphite */
};

static const wchar_t *const uiThemeNames[UI_THEME_COUNT] UI_UNUSED = {
    L"丁香紫", L"蒂芙尼蓝", L"熔岩橙", L"龙胆蓝", L"玫红",
    L"松叶绿", L"琥珀金", L"湖水蓝", L"暖咖棕", L"石墨灰"
};

static inline int uiThemeClamp(int theme) { return theme < 0 ? 0 : (theme >= UI_THEME_COUNT ? UI_THEME_COUNT - 1 : theme); }
static inline COLORREF uiThemeColor(int theme) { const unsigned char *t = uiThemeRgb[uiThemeClamp(theme)]; return RGB(t[0], t[1], t[2]); }

/* ------------------------------------------------------------- colour math */
static inline COLORREF uiBlend(COLORREF a, COLORREF b, int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return RGB((GetRValue(a) * (100 - percent) + GetRValue(b) * percent) / 100,
               (GetGValue(a) * (100 - percent) + GetGValue(b) * percent) / 100,
               (GetBValue(a) * (100 - percent) + GetBValue(b) * percent) / 100);
}
static inline COLORREF uiLighten(COLORREF c, int percent) { return uiBlend(c, RGB(255, 255, 255), percent); }
static inline COLORREF uiDarken(COLORREF c, int percent) { return uiBlend(c, RGB(0, 0, 0), percent); }

/* Perceived luminance (0..255) -- used to pick legible ink over an accent fill. */
static inline int uiLuminance(COLORREF c) {
    return (GetRValue(c) * 21 + GetGValue(c) * 72 + GetBValue(c) * 7) / 100;
}
/* Ink that stays readable when painted on top of `accent`. */
static inline COLORREF uiInkOn(COLORREF accent) {
    return uiLuminance(accent) < 140 ? RGB(255, 255, 255) : RGB(24, 26, 38);
}
/* Neutral wash of an accent, used for hovers and selection fills. */
static inline COLORREF uiAccentWash(COLORREF accent, int percent) { return uiBlend(UI_SURFACE, accent, percent); }

#endif /* LITELAUNCH_UI_THEME_H */
