#include "ui_theme.h"

static HWND pickOverlay;
static HWND lastPickHover;
static HBRUSH dialogBackground;
static HBRUSH inputBackground;

/* Settings, picker and blacklist surfaces all draw from the shared tokens in
   ui_theme.h so the panel, its dialogs and the EVER windows stay one system. */
static const COLORREF dialogColor=UI_CANVAS;
static const COLORREF inputColor=UI_SURFACE;
static const COLORREF dialogCard=UI_SURFACE_ALT;
static const COLORREF dialogInk=UI_INK;
static const COLORREF dialogInkSoft=UI_INK_SOFT;
static const COLORREF dialogMuted=UI_MUTED;
static const COLORREF dialogFaint=UI_FAINT;
static const COLORREF dialogBorder=UI_LINE;
static const COLORREF dialogBorderSoft=UI_LINE_SOFT;

/* GDI has no alpha blending here, so transparency is faked by blending toward
   the known backdrop colour. */
static void dialogFadeLine(HDC dc,int x1,int x2,int y,COLORREF background,COLORREF color,int intensity){
    const int segments=22;double span=(double)(x2-x1)/segments;
    for(int i=0;i<segments;i++){
        double p=(i+.5)/segments,a=1.0-fabs(p*2-1);int level=(int)(intensity*a);
        HPEN pen=CreatePen(PS_SOLID,1,uiBlend(background,color,level));HGDIOBJ old=SelectObject(dc,pen);
        MoveToEx(dc,x1+(int)(i*span),y,NULL);LineTo(dc,x1+(int)((i+1)*span)+1,y);
        SelectObject(dc,old);DeleteObject(pen);
    }
}

static void roundDialogRect(HDC dc,RECT r,int radius,COLORREF fill,COLORREF border,int borderWidth){
    HBRUSH brush=CreateSolidBrush(fill);HPEN pen=CreatePen(PS_SOLID,S(borderWidth),border);
    HGDIOBJ oldBrush=SelectObject(dc,brush),oldPen=SelectObject(dc,pen);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,S(radius*2),S(radius*2));
    SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);DeleteObject(brush);
}

static ARGB dialogArgb(COLORREF color){
    return 0xff000000u|((ARGB)GetRValue(color)<<16)|((ARGB)GetGValue(color)<<8)|GetBValue(color);
}

/* Native GDI RoundRect/Ellipse snap curved edges to pixels. Use the already
   initialized GDI+ renderer for the small controls where those steps show. */
static BOOL smoothDialogRoundRectW(HDC dc,RECT r,int radius,COLORREF fill,COLORREF border,REAL borderWidth,BOOL fillShape){
    GpGraphics *graphics=NULL;GpPath *path=NULL;GpSolidFill *brush=NULL;GpPen *pen=NULL;
    if(GdipCreateFromHDC(dc,&graphics)!=Ok)return FALSE;
    GdipSetSmoothingMode(graphics,SmoothingModeAntiAlias);
    REAL x=(REAL)r.left+0.5f,y=(REAL)r.top+0.5f;
    REAL width=(REAL)(r.right-r.left)-1.f,height=(REAL)(r.bottom-r.top)-1.f;
    REAL curve=(REAL)S(radius);if(curve*2.f>height)curve=height/2.f;
    if(curve*2.f>width)curve=width/2.f;
    BOOL drawn=FALSE;
    if(width>0.f&&height>0.f&&curve>0.f&&GdipCreatePath(FillModeAlternate,&path)==Ok){
        REAL diameter=curve*2.f;
        GdipAddPathArc(path,x+width-diameter,y,diameter,diameter,270.f,90.f);
        GdipAddPathArc(path,x+width-diameter,y+height-diameter,diameter,diameter,0.f,90.f);
        GdipAddPathArc(path,x,y+height-diameter,diameter,diameter,90.f,90.f);
        GdipAddPathArc(path,x,y,diameter,diameter,180.f,90.f);
        GdipClosePathFigure(path);
        if(fillShape&&GdipCreateSolidFill(dialogArgb(fill),&brush)==Ok)
            drawn=GdipFillPath(graphics,(GpBrush*)brush,path)==Ok;
        if(GdipCreatePen1(dialogArgb(border),borderWidth,UnitPixel,&pen)==Ok)
            drawn=GdipDrawPath(graphics,pen,path)==Ok||drawn;
    }
    if(pen)GdipDeletePen(pen);if(brush)GdipDeleteBrush((GpBrush*)brush);
    if(path)GdipDeletePath(path);GdipDeleteGraphics(graphics);
    return drawn;
}
static BOOL smoothDialogRoundRect(HDC dc,RECT r,int radius,COLORREF fill,COLORREF border,BOOL fillShape){
    return smoothDialogRoundRectW(dc,r,radius,fill,border,1.f,fillShape);
}

/* Vertical gradient version. Filled primary buttons use this instead of a 1px
   light bar painted across the top: that bar ran outside the rounded corners
   and read as a stray line floating above the button. */
static BOOL smoothDialogRoundRectGradient(HDC dc,RECT r,int radius,COLORREF top,COLORREF bottom,COLORREF border,REAL borderWidth){
    GpGraphics *graphics=NULL;GpPath *path=NULL;GpLineGradient *brush=NULL;GpPen *pen=NULL;
    if(GdipCreateFromHDC(dc,&graphics)!=Ok)return FALSE;
    GdipSetSmoothingMode(graphics,SmoothingModeAntiAlias);
    REAL x=(REAL)r.left+0.5f,y=(REAL)r.top+0.5f;
    REAL width=(REAL)(r.right-r.left)-1.f,height=(REAL)(r.bottom-r.top)-1.f;
    REAL curve=(REAL)S(radius);if(curve*2.f>height)curve=height/2.f;
    if(curve*2.f>width)curve=width/2.f;
    BOOL drawn=FALSE;
    if(width>0.f&&height>0.f&&curve>0.f&&GdipCreatePath(FillModeAlternate,&path)==Ok){
        REAL diameter=curve*2.f;
        GdipAddPathArc(path,x+width-diameter,y,diameter,diameter,270.f,90.f);
        GdipAddPathArc(path,x+width-diameter,y+height-diameter,diameter,diameter,0.f,90.f);
        GdipAddPathArc(path,x,y+height-diameter,diameter,diameter,90.f,90.f);
        GdipAddPathArc(path,x,y,diameter,diameter,180.f,90.f);
        GdipClosePathFigure(path);
        GpRectF bounds={x,y,width,height};
        if(GdipCreateLineBrushFromRect(&bounds,dialogArgb(top),dialogArgb(bottom),LinearGradientModeVertical,WrapModeTile,&brush)==Ok)
            drawn=GdipFillPath(graphics,(GpBrush*)brush,path)==Ok;
        if(GdipCreatePen1(dialogArgb(border),borderWidth,UnitPixel,&pen)==Ok)
            drawn=GdipDrawPath(graphics,pen,path)==Ok||drawn;
    }
    if(pen)GdipDeletePen(pen);if(brush)GdipDeleteBrush((GpBrush*)brush);
    if(path)GdipDeletePath(path);GdipDeleteGraphics(graphics);
    return drawn;
}

static BOOL smoothDialogCircle(HDC dc,RECT r,COLORREF fill,COLORREF border){
    GpGraphics *graphics=NULL;GpSolidFill *brush=NULL;GpPen *pen=NULL;
    if(GdipCreateFromHDC(dc,&graphics)!=Ok)return FALSE;
    GdipSetSmoothingMode(graphics,SmoothingModeAntiAlias);
    REAL x=(REAL)r.left+0.5f,y=(REAL)r.top+0.5f;
    REAL width=(REAL)(r.right-r.left)-1.f,height=(REAL)(r.bottom-r.top)-1.f;
    BOOL drawn=FALSE;
    if(GdipCreateSolidFill(dialogArgb(fill),&brush)==Ok)
        drawn=GdipFillEllipse(graphics,(GpBrush*)brush,x,y,width,height)==Ok;
    if(GdipCreatePen1(dialogArgb(border),1.f,UnitPixel,&pen)==Ok)
        drawn=GdipDrawEllipse(graphics,pen,x,y,width,height)==Ok||drawn;
    if(pen)GdipDeletePen(pen);if(brush)GdipDeleteBrush((GpBrush*)brush);
    GdipDeleteGraphics(graphics);return drawn;
}

static void roundDialogWindow(HWND w,int radius){
    RECT r;GetClientRect(w,&r);
    HRGN region=CreateRoundRectRgn(0,0,r.right+1,r.bottom+1,S(radius*2),S(radius*2));
    if(region&&!SetWindowRgn(w,region,TRUE))DeleteObject(region);
}

/* A native EDIT/LISTBOX is a square child window painted over a *rounded*
   field. Its square corners therefore paint over the field's border arc, which
   reads as the white bar being longer than the frame that is supposed to hold
   it. Clipping the child to the same rounded shape keeps the outline intact at
   every scale factor. Safe to call again after a resize. */
static void roundFieldControl(HWND control){
    if(!control||!IsWindow(control))return;
    RECT r;GetClientRect(control,&r);
    if(r.right<=0||r.bottom<=0)return;
    int curve=S(UI_R_FIELD*2);
    if(curve<2)curve=2;
    if(curve>r.bottom)curve=r.bottom;
    if(curve>r.right)curve=r.right;
    HRGN region=CreateRoundRectRgn(0,0,r.right+1,r.bottom+1,curve,curve);
    if(region&&!SetWindowRgn(control,region,TRUE))DeleteObject(region);
}

static void paintDialogHeader(HDC dc,RECT bounds,LPCWSTR title,LPCWSTR eyebrow){
    RECT header=bounds;header.bottom=S(GLASS_HEADER_HEIGHT);
    HBRUSH white=CreateSolidBrush(inputColor);FillRect(dc,&header,white);DeleteObject(white);
    /* Short accent pill replaces the old hard-edged bar. */
    RECT mark={S(18),S(16),S(22),S(40)};
    if(!smoothDialogRoundRect(dc,mark,2,glassAccentColor(),glassAccentColor(),TRUE))
        roundDialogRect(dc,mark,2,glassAccentColor(),glassAccentColor(),1);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,dialogMuted);SelectObject(dc,metaFont);
    RECT small={S(32),S(7),bounds.right-S(200),S(22)};
    DrawTextW(dc,eyebrow,-1,&small,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    SetTextColor(dc,dialogInk);SelectObject(dc,titleFont);
    RECT heading={S(32),S(22),bounds.right-S(200),S(50)};
    DrawTextW(dc,title,-1,&heading,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    /* Bottom hairline fades out at both ends instead of cutting the header off. */
    dialogFadeLine(dc,S(18),bounds.right-S(18),S(GLASS_HEADER_HEIGHT),inputColor,dialogBorder,255);
}

/* Fields read as soft white cards. The fill stays white because the native
   EDIT inside is white: tinting the field made the edit rectangle read as a
   separate block breaking out of its frame. The frame carries the state. */
static void paintDialogField(HDC dc,int x,int y,int w,int h,BOOL focused){
    RECT r={S(x),S(y),S(x+w),S(y+h)};
    COLORREF border=focused?glassAccentColor():uiBlend(dialogBorder,dialogInk,20);
    if(!smoothDialogRoundRectW(dc,r,UI_R_FIELD,inputColor,border,focused?1.8f:1.2f,TRUE))
        roundDialogRect(dc,r,UI_R_FIELD,inputColor,border,1);
    if(focused){
        /* A one pixel accent halo outside the field marks focus from a distance. */
        RECT halo=r;InflateRect(&halo,-S(2),-S(2));
        smoothDialogRoundRectW(dc,halo,UI_R_FIELD-2,0,uiBlend(inputColor,glassAccentColor(),45),1.f,FALSE);
    }
}

static void paintDialogStatusLine(HDC dc,LPCWSTR value,int y,int width){
    if(!value||!value[0])return;
    RECT dot={S(26),S(y+5),S(32),S(y+11)};
    if(!smoothDialogCircle(dc,dot,glassAccentColor(),glassAccentColor())){
        HBRUSH accent=CreateSolidBrush(glassAccentColor());FillRect(dc,&dot,accent);DeleteObject(accent);}
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,dialogMuted);SelectObject(dc,font);
    RECT label={S(40),S(y),width-S(24),S(y+20)};
    DrawTextW(dc,value,-1,&label,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
}

static BOOL drawModernButton(DRAWITEMSTRUCT *item){
    if(!item||item->CtlType!=ODT_BUTTON)return FALSE;
    RECT r=item->rcItem,screen;
    GetWindowRect(item->hwndItem,&screen);
    POINT origin={screen.left,screen.top};ScreenToClient(GetParent(item->hwndItem),&origin);
    FillRect(item->hDC,&r,origin.y<S(GLASS_HEADER_HEIGHT)?inputBackground:dialogBackground);
    WCHAR label[96]=L"";GetWindowTextW(item->hwndItem,label,96);
    BOOL close=label[0]==L'×',hot=(item->itemState&ODS_HOTLIGHT)!=0,focused=(item->itemState&ODS_FOCUS)!=0;
    BOOL disabled=(item->itemState&ODS_DISABLED)!=0,pressed=(item->itemState&ODS_SELECTED)!=0;
    COLORREF accent=glassAccentColor();
    if(origin.y<S(GLASS_HEADER_HEIGHT)){
        /* Title-bar buttons sit on a soft plate that only appears on hover,
           except labelled header actions, which stay visible as real buttons. */
        RECT plate=r;InflateRect(&plate,-S(1),-S(3));
        BOOL plateHot=hot&&!disabled;
        if(!close){
            COLORREF fill=plateHot?uiBlend(inputColor,accent,10):inputColor;
            COLORREF line=plateHot?uiBlend(dialogBorder,accent,46):dialogBorder;
            if(pressed&&!disabled)fill=uiBlend(fill,accent,14);
            if(!smoothDialogRoundRect(item->hDC,plate,UI_R_PILL,fill,line,TRUE))
                roundDialogRect(item->hDC,plate,UI_R_PILL,fill,line,1);
            SetBkMode(item->hDC,TRANSPARENT);
            SetTextColor(item->hDC,disabled?dialogFaint:(plateHot?uiDarken(accent,6):uiBlend(dialogInk,accent,34)));
            SelectObject(item->hDC,font);RECT text=plate;if(pressed)OffsetRect(&text,0,S(1));
            DrawTextW(item->hDC,label,-1,&text,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
            return TRUE;
        }
        if(plateHot||(focused&&!disabled)){
            COLORREF wash=RGB(255,238,240),line=UI_DANGER_LINE;
            if(!smoothDialogRoundRect(item->hDC,plate,UI_R_PILL,wash,line,TRUE))
                roundDialogRect(item->hDC,plate,UI_R_PILL,wash,line,1);
        }
        COLORREF color=disabled?dialogFaint:(plateHot?UI_ACCENT_WARM:dialogMuted);
        SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,color);
        SelectObject(item->hDC,symbolFont);
        RECT text=r;if(pressed)OffsetRect(&text,0,S(1));
        DrawTextW(item->hDC,label,-1,&text,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
        return TRUE;
    }
    InflateRect(&r,-S(1),-S(1));
    WCHAR cls[48]=L"";GetClassNameW(GetParent(item->hwndItem),cls,48);
    BOOL inBlacklist=!wcscmp(cls,L"LiteLaunch.Blacklist");
    BOOL primary=inBlacklist?item->CtlID==103:item->CtlID==IDOK;
    BOOL danger=inBlacklist&&item->CtlID==105;
    COLORREF fill=inputColor,border=dialogBorder,ink=dialogInk;
    if(primary){fill=accent;border=accent;ink=uiInkOn(accent);}
    else if(hot&&!disabled){fill=danger?UI_DANGER_SOFT:uiBlend(dialogCard,accent,9);border=danger?UI_DANGER_LINE:uiBlend(dialogBorder,accent,52);}
    else fill=dialogCard;
    if(pressed&&!disabled)fill=primary?uiDarken(accent,9):uiBlend(fill,accent,15);
    if(disabled){fill=dialogCard;border=dialogBorderSoft;ink=dialogFaint;}
    if(primary&&!disabled){
        COLORREF top=pressed?uiDarken(accent,5):uiLighten(accent,20);
        COLORREF bottom=pressed?uiDarken(accent,15):accent;
        if(!smoothDialogRoundRectGradient(item->hDC,r,UI_R_BUTTON,top,bottom,border,1.f))
            roundDialogRect(item->hDC,r,UI_R_BUTTON,fill,border,1);
    }else if(!smoothDialogRoundRect(item->hDC,r,UI_R_BUTTON,fill,border,TRUE))
        roundDialogRect(item->hDC,r,UI_R_BUTTON,fill,border,1);
    if((item->itemState&ODS_FOCUS)&&!disabled){RECT focus=r;InflateRect(&focus,-S(3),-S(3));
        COLORREF ring=primary?uiInkOn(accent):accent;
        if(!smoothDialogRoundRect(item->hDC,focus,UI_R_BUTTON-3,fill,ring,FALSE)){
            HPEN pen=CreatePen(PS_SOLID,S(1),ring);HGDIOBJ oldPen=SelectObject(item->hDC,pen),oldBrush=SelectObject(item->hDC,GetStockObject(HOLLOW_BRUSH));
            RoundRect(item->hDC,focus.left,focus.top,focus.right,focus.bottom,S(17),S(17));
            SelectObject(item->hDC,oldBrush);SelectObject(item->hDC,oldPen);DeleteObject(pen);}}
    SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,ink);
    SelectObject(item->hDC,close?symbolFont:font);
    if(pressed)OffsetRect(&r,0,S(1));
    DrawTextW(item->hDC,label,-1,&r,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);return TRUE;
}

static BOOL drawDialogStatus(DRAWITEMSTRUCT *item){
    if(!item||item->CtlType!=ODT_STATIC)return FALSE;
    RECT r=item->rcItem;FillRect(item->hDC,&r,dialogBackground);
    RECT card=r;InflateRect(&card,-S(1),-S(1));
    if(!smoothDialogRoundRect(item->hDC,card,UI_R_FIELD,inputColor,dialogBorder,TRUE))
        roundDialogRect(item->hDC,card,UI_R_FIELD,inputColor,dialogBorder,1);
    RECT mark={card.left+S(6),card.top+S(9),card.left+S(10),card.bottom-S(9)};
    COLORREF accent=glassAccentColor();
    if(!smoothDialogRoundRect(item->hDC,mark,2,accent,accent,TRUE))roundDialogRect(item->hDC,mark,2,accent,accent,1);
    WCHAR value[2300]=L"";GetWindowTextW(item->hwndItem,value,2300);
    RECT textRect={card.left+S(20),card.top+S(6),card.right-S(10),card.bottom-S(6)};
    SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,dialogInk);SelectObject(item->hDC,font);
    DrawTextW(item->hDC,value,-1,&textRect,DT_WORDBREAK|DT_END_ELLIPSIS|DT_NOPREFIX);return TRUE;
}

static LRESULT CALLBACK PickerProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_NCHITTEST)return HTTRANSPARENT;
    if(msg==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);WCHAR type[32];GetClassNameW(w,type,32);
        if(!wcscmp(type,L"LiteLaunch.PickOverlay")){HBRUSH red=CreateSolidBrush(RGB(239,60,72));FillRect(dc,&r,red);DeleteObject(red);}
        else{
            HBRUSH white=CreateSolidBrush(inputColor);FillRect(dc,&r,white);DeleteObject(white);
            if(!smoothDialogRoundRect(dc,r,14,inputColor,dialogBorder,TRUE))roundDialogRect(dc,r,14,inputColor,dialogBorder,1);
            COLORREF accent=glassAccentColor();
            RECT mark={S(18),S(19),S(22),S(59)};
            if(!smoothDialogRoundRect(dc,mark,2,accent,accent,TRUE))roundDialogRect(dc,mark,2,accent,accent,1);
            SetBkMode(dc,TRANSPARENT);SetTextColor(dc,dialogInk);SelectObject(dc,titleFont);
            RECT title={S(32),S(10),r.right-S(20),S(40)};
            DrawTextW(dc,blacklistPicking?L"选择要屏蔽的应用":L"选择要添加的应用",-1,&title,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
            SelectObject(dc,font);SetTextColor(dc,dialogMuted);
            RECT sub={S(32),S(41),r.right-S(20),S(67)};
            DrawTextW(dc,L"点击目标窗口确认 · 右键或Esc取消",-1,&sub,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        }EndPaint(w,&ps);return 0;
    }return DefWindowProcW(w,msg,wp,lp);
}

static void highlightPick(HWND target){
    if(!picking)return;DWORD pid=0;GetWindowThreadProcessId(target,&pid);
    if(!target||pid==GetCurrentProcessId()){if(pickOverlay)ShowWindow(pickOverlay,SW_HIDE);return;}
    RECT r;if(!GetWindowRect(target,&r))return;
    if(!pickOverlay){pickOverlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,L"LiteLaunch.PickOverlay",L"",WS_POPUP|WS_DISABLED,0,0,0,0,NULL,NULL,instance,NULL);SetLayeredWindowAttributes(pickOverlay,0,52,LWA_ALPHA);}
    SetWindowPos(pickOverlay,HWND_TOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    if(pickTip)SetWindowPos(pickTip,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
}

static void startPickerVisual(void){
    WNDCLASSW wc={0};wc.hInstance=instance;wc.lpfnWndProc=PickerProc;wc.lpszClassName=L"LiteLaunch.PickOverlay";RegisterClassW(&wc);wc.lpszClassName=L"LiteLaunch.PickHint";RegisterClassW(&wc);
    POINT p;GetCursorPos(&p);MONITORINFO mi={0};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST),&mi);
    int width=S(440);pickTip=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT,L"LiteLaunch.PickHint",L"",WS_POPUP|WS_DISABLED,mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-width)/2,mi.rcWork.top+S(24),width,S(78),NULL,NULL,instance,NULL);
    SetWindowRgn(pickTip,CreateRoundRectRgn(0,0,width+1,S(78)+1,S(28),S(28)),FALSE);ShowWindow(pickTip,SW_SHOWNOACTIVATE);lastPickHover=NULL;highlightPick(GetAncestor(WindowFromPoint(p),GA_ROOT));
}

static void stopPickerVisual(void){if(pickOverlay){DestroyWindow(pickOverlay);pickOverlay=NULL;}if(pickTip){DestroyWindow(pickTip);pickTip=NULL;}lastPickHover=NULL;}

static LRESULT dialogColors(UINT msg,WPARAM wp){
    HDC dc=(HDC)wp;
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORDLG||msg==WM_CTLCOLORBTN){SetBkColor(dc,dialogColor);SetTextColor(dc,dialogInkSoft);return (LRESULT)dialogBackground;}
    if(msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX){SetBkColor(dc,inputColor);SetTextColor(dc,dialogInk);return (LRESULT)inputBackground;}return 0;
}
