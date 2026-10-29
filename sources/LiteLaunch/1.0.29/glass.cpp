#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cwctype>
#include <gdiplus.h>
#include <math.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <algorithm>
#include "glass.h"
#include "ui_theme.h"
#define NANOSVG_IMPLEMENTATION
#include "vendor/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "vendor/nanosvgrast.h"
using namespace Gdiplus;
static ULONG_PTR token;
static Bitmap *backdrop;
static WCHAR base[MAX_PATH];
static Bitmap *chrome[5];
static Bitmap *pluginChrome[3];
static Bitmap *cardShadow;
static float cardShadowTileW,cardShadowTileH;
static Bitmap *cardLayer;
static bool cardLayerValid;
static int cardLayerW,cardLayerH,cardLayerRows,cardLayerTheme,cardLayerSlots,cardLayerOffset;
static unsigned cardLayerSignature;
static int chromeDpi;
static bool contains(const WCHAR *text,const WCHAR *part){std::wstring a=text,b=part;std::transform(a.begin(),a.end(),a.begin(),towlower);std::transform(b.begin(),b.end(),b.begin(),towlower);return a.find(b)!=std::wstring::npos;}
static const BYTE *resourceBytes(WORD id,DWORD *length){
    HMODULE module=GetModuleHandleW(nullptr);HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
    if(!resource)return nullptr;*length=SizeofResource(module,resource);HGLOBAL loaded=LoadResource(module,resource);
    return loaded?(const BYTE*)LockResource(loaded):nullptr;
}
static Bitmap *rasterizeSvg(char *text,int size){
    NSVGimage *svg=nsvgParse(text,"px",96);if(!svg||svg->width<=0||svg->height<=0){if(svg)nsvgDelete(svg);return nullptr;}
    NSVGrasterizer *r=nsvgCreateRasterizer();if(!r){nsvgDelete(svg);return nullptr;}
    std::vector<unsigned char> rgba(size*size*4);float scale=std::min(size/svg->width,size/svg->height);
    nsvgRasterize(r,svg,(size-svg->width*scale)/2,(size-svg->height*scale)/2,scale,rgba.data(),size,size,size*4);
    nsvgDeleteRasterizer(r);nsvgDelete(svg);
    Bitmap *bmp=new Bitmap(size,size,PixelFormat32bppARGB);BitmapData lock;Rect rect(0,0,size,size);
    if(bmp->LockBits(&rect,ImageLockModeWrite,PixelFormat32bppARGB,&lock)!=Ok){delete bmp;return nullptr;}
    for(int y=0;y<size;y++){unsigned char *d=(unsigned char*)lock.Scan0+y*lock.Stride;for(int x=0;x<size;x++){int k=(y*size+x)*4;d[x*4]=rgba[k+2];d[x*4+1]=rgba[k+1];d[x*4+2]=rgba[k];d[x*4+3]=rgba[k+3];}}
    bmp->UnlockBits(&lock);return bmp;
}
static Bitmap *readSvg(const WCHAR *file,int size){
    FILE *f=_wfopen(file,L"rb");if(!f)return nullptr;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
    if(n<=0||n>2*1024*1024){fclose(f);return nullptr;}std::vector<char> text(n+1);size_t got=fread(text.data(),1,n,f);fclose(f);if(got!=(size_t)n)return nullptr;
    return rasterizeSvg(text.data(),size);
}
static Bitmap *resourceSvg(WORD id,int size){
    DWORD length=0;const BYTE *bytes=resourceBytes(id,&length);if(!bytes||!length||length>2*1024*1024)return nullptr;
    std::vector<char> text(length+1);memcpy(text.data(),bytes,length);return rasterizeSvg(text.data(),size);
}
static Bitmap *resourcePng(WORD id){
    DWORD length=0;const BYTE *bytes=resourceBytes(id,&length);if(!bytes||!length)return nullptr;
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,length);if(!memory)return nullptr;
    void *destination=GlobalLock(memory);if(!destination){GlobalFree(memory);return nullptr;}
    memcpy(destination,bytes,length);GlobalUnlock(memory);
    IStream *stream=nullptr;if(CreateStreamOnHGlobal(memory,TRUE,&stream)!=S_OK){GlobalFree(memory);return nullptr;}
    Bitmap *encoded=Bitmap::FromStream(stream,FALSE),*result=nullptr;
    if(encoded&&encoded->GetLastStatus()==Ok)result=encoded->Clone(0,0,encoded->GetWidth(),encoded->GetHeight(),PixelFormat32bppARGB);
    delete encoded;stream->Release();return result;
}
static Bitmap *asset(const WCHAR *name,int size){
    const WCHAR *pluginIds[]={L"plugin-renamer",L"plugin-cursor",L"plugin-add"};
    for(WORD i=0;i<3;i++)if(!wcscmp(name,pluginIds[i])){Bitmap *embedded=resourceSvg(107+i,size);if(embedded)return embedded;break;}
    const WCHAR *ids[]={L"logo",L"pin",L"gear",L"close",L"plus"};
    for(WORD i=0;i<5;i++)if(!wcscmp(name,ids[i])){Bitmap *embedded=resourceSvg(101+i,size);if(embedded)return embedded;break;}
    WCHAR path[1024];swprintf(path,1024,L"%lsicons\\%ls.svg",base,name);return readSvg(path,size);
}
static Bitmap *officialIconBitmap(HICON icon){
    ICONINFO info={};if(!GetIconInfo(icon,&info))return Bitmap::FromHICON(icon);
    BITMAP bm={};Bitmap *result=nullptr;
    if(info.hbmColor&&GetObjectW(info.hbmColor,sizeof(bm),&bm)&&bm.bmBitsPixel==32){
        BITMAPINFO bi={};bi.bmiHeader.biSize=40;bi.bmiHeader.biWidth=bm.bmWidth;bi.bmiHeader.biHeight=-bm.bmHeight;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
        std::vector<unsigned char> data(bm.bmWidth*bm.bmHeight*4);HDC dc=GetDC(nullptr);
        int rows=GetDIBits(dc,info.hbmColor,0,bm.bmHeight,data.data(),&bi,DIB_RGB_COLORS);ReleaseDC(nullptr,dc);
        bool alpha=false;for(size_t i=3;i<data.size();i+=4)if(data[i]){alpha=true;break;}
        if(rows&&alpha){result=new Bitmap(bm.bmWidth,bm.bmHeight,PixelFormat32bppPARGB);BitmapData lock;Rect r(0,0,bm.bmWidth,bm.bmHeight);
            if(result->LockBits(&r,ImageLockModeWrite,PixelFormat32bppPARGB,&lock)==Ok){for(int y=0;y<bm.bmHeight;y++)memcpy((unsigned char*)lock.Scan0+y*lock.Stride,data.data()+y*bm.bmWidth*4,bm.bmWidth*4);result->UnlockBits(&lock);}else{delete result;result=nullptr;}}
    }
    if(info.hbmColor)DeleteObject(info.hbmColor);if(info.hbmMask)DeleteObject(info.hbmMask);
    return result?result:Bitmap::FromHICON(icon);
}
extern "C" void glassInit(){GdiplusStartupInput input;GdiplusStartup(&token,&input,nullptr);GetModuleFileNameW(nullptr,base,MAX_PATH);WCHAR *p=wcsrchr(base,L'\\');if(p)p[1]=0;}
extern "C" void glassFreeIcon(void *p){delete (Bitmap*)p;}
extern "C" BOOL glassExportIcon(void *icon,const WCHAR *path){if(!icon)return FALSE;UINT n=0,bytes=0;GetImageEncodersSize(&n,&bytes);std::vector<unsigned char> data(bytes);auto enc=(ImageCodecInfo*)data.data();GetImageEncoders(n,bytes,enc);for(UINT i=0;i<n;i++)if(!wcscmp(enc[i].MimeType,L"image/png"))return ((Bitmap*)icon)->Save(path,&enc[i].Clsid,nullptr)==Ok;return FALSE;}
extern "C" void glassShutdown(){delete backdrop;backdrop=nullptr;delete cardShadow;cardShadow=nullptr;cardShadowTileW=cardShadowTileH=0;delete cardLayer;cardLayer=nullptr;cardLayerValid=false;for(auto &p:chrome){delete p;p=nullptr;}for(auto &p:pluginChrome){delete p;p=nullptr;}chromeDpi=0;if(token)GdiplusShutdown(token);}
extern "C" void *glassLoadControlPanelIcon(void){
    PIDLIST_ABSOLUTE folder=nullptr;Bitmap *bitmap=nullptr;
    if(SUCCEEDED(SHParseDisplayName(L"::{26EE0668-A00A-44D7-9371-BEB064C98683}",nullptr,&folder,0,nullptr))){
        SHFILEINFOW info={};if(SHGetFileInfoW((LPCWSTR)folder,0,&info,sizeof(info),SHGFI_PIDL|SHGFI_ICON|SHGFI_LARGEICON)&&info.hIcon){bitmap=officialIconBitmap(info.hIcon);DestroyIcon(info.hIcon);}CoTaskMemFree(folder);
    }
    return bitmap;
}
extern "C" void *glassLoadIcon(const WCHAR *path,const WCHAR *name,const WCHAR *target,int dpi){
    int size=MulDiv(38,dpi,96);Bitmap *b=nullptr;
    const WCHAR *ext=wcsrchr(path,L'.');if(ext&&!_wcsicmp(ext,L".svg")){WCHAR expanded[2048];ExpandEnvironmentStringsW(path,expanded,2048);if(PathIsRelativeW(expanded)){WCHAR joined[2048];swprintf(joined,2048,L"%ls%ls",base,expanded);b=readSvg(joined,size);}else b=readSvg(expanded,size);if(b)return b;}
    if(contains(target,L"OpenAI.Codex")||contains(name,L"Codex")){
        WCHAR official[1024];swprintf(official,1024,L"%lsicons\\codex-official-legacy.png",base);
        b=new Bitmap(official);if(b->GetLastStatus()==Ok)return b;delete b;b=nullptr;
        b=resourcePng(106);if(b)return b;
    }
    WCHAR expanded[2048];ExpandEnvironmentStringsW(path[0]?path:target,expanded,2048);
    ext=wcsrchr(expanded,L'.');
    if(ext&&(!_wcsicmp(ext,L".png")||!_wcsicmp(ext,L".jpg"))){b=new Bitmap(expanded);if(b->GetLastStatus()==Ok)return b;delete b;b=nullptr;
        if(contains(expanded,L"codex-official-legacy.png")){b=resourcePng(106);if(b)return b;}}
    HICON hi=nullptr;UINT iconId=0;
    UINT count=PrivateExtractIconsW(expanded,0,std::max(64,size),std::max(64,size),&hi,&iconId,1,0);
    if(!count||count==UINT_MAX||!hi){SHFILEINFOW info={};SHGetFileInfoW(expanded,0,&info,sizeof(info),SHGFI_ICON|SHGFI_LARGEICON);hi=info.hIcon;}
    if(hi){b=officialIconBitmap(hi);DestroyIcon(hi);if(b&&b->GetLastStatus()==Ok)return b;delete b;}
    return nullptr;
}
static bool backgroundDirty=true, animating=false;
static int themeIndex=0;
/* Accent palette lives in ui_theme.h so the panel, dialog swatches and menu
   swatches can never drift apart. Index order is the persisted contract. */
static const unsigned char *accentRgb(void){return uiThemeRgb[themeIndex];}
static Color accentColor(int alpha){const unsigned char *t=accentRgb();return Color((BYTE)alpha,t[0],t[1],t[2]);}
extern "C" void glassTheme(int theme){themeIndex=std::clamp(theme,0,9);backgroundDirty=true;}
extern "C" COLORREF glassAccentColor(void){const unsigned char *t=accentRgb();return RGB(t[0],t[1],t[2]);}
extern "C" HBITMAP glassThemeSwatch(int theme,int size){
    BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=size;bi.bmiHeader.biHeight=-size;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    void *pixels=nullptr;HBITMAP result=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);if(!result)return nullptr;
    memset(pixels,0,size*size*4);Bitmap bitmap(size,size,size*4,PixelFormat32bppPARGB,(BYTE*)pixels);Graphics g(&bitmap);g.SetSmoothingMode(SmoothingModeAntiAlias);
    const unsigned char *t=uiThemeRgb[uiThemeClamp(theme)];
    /* A tiny sphere: vertical gradient body, soft highlight, hairline rim. */
    LinearGradientBrush body(RectF(2.f,2.f,size-4.f,size-4.f),Color(255,(BYTE)std::min(255,t[0]+38),(BYTE)std::min(255,t[1]+38),(BYTE)std::min(255,t[2]+38)),Color(255,(BYTE)(t[0]*3/4),(BYTE)(t[1]*3/4),(BYTE)(t[2]*3/4)),LinearGradientModeVertical);
    g.FillEllipse(&body,2.f,2.f,size-4.f,size-4.f);
    SolidBrush gloss(Color(96,255,255,255));g.FillEllipse(&gloss,size*.26f,size*.18f,size*.30f,size*.22f);
    Pen rim(Color(46,0,0,0),1.f);g.DrawEllipse(&rim,2.5f,2.5f,size-5.f,size-5.f);
    g.Flush(FlushIntentionSync);return result;
}
static unsigned long long previousHash=0;
static float lensX=8,lensY=40,lensVisibility=0;
static ULONGLONG lastFrame=0;
static int lastActive=-1;
static int slotCount=16,rowOffset=0;

static float pointerX=-100,pointerY=-100,iconX[GLASS_SLOT_CAPACITY],iconY[GLASS_SLOT_CAPACITY];
static int dragFrom=-1,dragTo=-1;static bool iconsReady=false;
static int slotRow(int slot){return slot<GLASS_FUNCTION_SLOTS?0:slot/4-rowOffset;}
static bool visibleSlot(int slot,int rows){if(slot<0||slot>=slotCount)return false;int row=slotRow(slot);return slot<GLASS_FUNCTION_SLOTS||(row>=1&&row<rows);}
static bool interactiveSlot(int slot){return slot>=0&&slot<slotCount&&(slot<2||slot>=GLASS_FUNCTION_SLOTS);}
extern "C" void glassRows(int count,int offset){count=std::clamp(count,8,(int)GLASS_SLOT_CAPACITY);offset=std::max(0,offset);if(slotCount!=count||rowOffset!=offset)iconsReady=false;slotCount=count;rowOffset=offset;}
extern "C" void glassInteraction(float x,float y,int from,int to){pointerX=x;pointerY=y;dragFrom=from>=GLASS_FUNCTION_SLOTS&&from<slotCount?from:-1;dragTo=to>=GLASS_FUNCTION_SLOTS&&to<slotCount?to:-1;}
extern "C" void glassReorder(int from,int to){if(from<GLASS_FUNCTION_SLOTS||to<GLASS_FUNCTION_SLOTS||from>=slotCount||to>=slotCount||from==to)return;float x=iconX[from],y=iconY[from];if(from<to){for(int i=from;i<to;i++){iconX[i]=iconX[i+1];iconY[i]=iconY[i+1];}}else for(int i=from;i>to;i--){iconX[i]=iconX[i-1];iconY[i]=iconY[i-1];}iconX[to]=x;iconY[to]=y;}

extern "C" void glassResetSelection(){lastActive=-1;lensVisibility=0;lastFrame=0;animating=false;iconsReady=false;dragFrom=dragTo=-1;}
extern "C" BOOL glassNeedsPaint(){return backgroundDirty||animating;}
extern "C" void glassCapture(int x,int y,int w,int h){
    int sw=std::max(1,w/4),sh=std::max(1,h/4);HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=sw;bi.bmiHeader.biHeight=-sh;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *pixels=nullptr;HBITMAP hb=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);if(!hb){DeleteDC(dc);ReleaseDC(nullptr,screen);return;}
    HGDIOBJ prev=SelectObject(dc,hb);SetStretchBltMode(dc,HALFTONE);BOOL captured=StretchBlt(dc,0,0,sw,sh,screen,x,y,w,h,SRCCOPY);GdiFlush();
    if(!captured){SelectObject(dc,prev);DeleteObject(hb);DeleteDC(dc);ReleaseDC(nullptr,screen);return;}
    auto bytes=(unsigned char*)pixels;unsigned long long hash=1469598103934665603ULL;
    for(int i=0;i<sw*sh*4;i+=4){hash^=(unsigned long long)(bytes[i]>>2)|((unsigned long long)(bytes[i+1]>>2)<<8)|((unsigned long long)(bytes[i+2]>>2)<<16);hash*=1099511628211ULL;}
    hash^=((unsigned long long)sw<<32)|sh;
    if(hash==previousHash&&backdrop){SelectObject(dc,prev);DeleteObject(hb);DeleteDC(dc);ReleaseDC(nullptr,screen);return;}
    previousHash=hash;backgroundDirty=true;
    std::vector<float> tmp(sw*sh*3);std::vector<unsigned char> out(sw*sh*4);float kernel[25],sum=0;
    for(int i=-12;i<=12;i++){kernel[i+12]=expf(-i*i/32.0f);sum+=kernel[i+12];}for(float &v:kernel)v/=sum;
    auto src=(unsigned char*)pixels;
    for(int yy=0;yy<sh;yy++)for(int xx=0;xx<sw;xx++)for(int c=0;c<3;c++){float v=0;for(int k=-12;k<=12;k++)v+=src[(yy*sw+std::clamp(xx+k,0,sw-1))*4+c]*kernel[k+12];tmp[(yy*sw+xx)*3+c]=v;}
    for(int yy=0;yy<sh;yy++)for(int xx=0;xx<sw;xx++){for(int c=0;c<3;c++){float v=0;for(int k=-12;k<=12;k++)v+=tmp[(std::clamp(yy+k,0,sh-1)*sw+xx)*3+c]*kernel[k+12];out[(yy*sw+xx)*4+c]=(unsigned char)v;}out[(yy*sw+xx)*4+3]=255;}
    delete backdrop;backdrop=new Bitmap(sw,sh,PixelFormat32bppARGB);BitmapData lock;Rect rect(0,0,sw,sh);
    if(backdrop->LockBits(&rect,ImageLockModeWrite,PixelFormat32bppARGB,&lock)==Ok){for(int yy=0;yy<sh;yy++)memcpy((unsigned char*)lock.Scan0+yy*lock.Stride,out.data()+yy*sw*4,sw*4);backdrop->UnlockBits(&lock);}
    SelectObject(dc,prev);DeleteObject(hb);DeleteDC(dc);ReleaseDC(nullptr,screen);
}
// Quartic superellipse corners meet straight edges with zero curvature and zero
// curvature slope. One field drives the mask, stroke, and lens normal.
static float roundedDistance(float x,float y,float w,float h,float r){
    r=std::min(r,std::min(w,h)/2);
    float sx=fabsf(x-w*.5f)-(w*.5f-r),sy=fabsf(y-h*.5f)-(h*.5f-r);
    float qx=std::max(sx,0.f),qy=std::max(sy,0.f);
    float n=sqrtf(sqrtf(qx*qx*qx*qx+qy*qy*qy*qy));
    if(n<.0001f)return std::max(sx,sy)-r;
    float grad=sqrtf(qx*qx*qx*qx*qx*qx+qy*qy*qy*qy*qy*qy)/(n*n*n);
    return (n-r)/std::max(grad,.0001f);
}
static void rounded(GraphicsPath &p,float x,float y,float w,float h,float r){
    r=std::min(r,std::min(w,h)/2);PointF pts[260];int n=0;
    for(int corner=0;corner<4;corner++)for(int j=0;j<=64;j++){
        float t=(corner*90+j*90.f/64)*3.14159265359f/180,cs=cosf(t),sn=sinf(t);
        float cx=(corner==0||corner==3)?x+w-r:x+r,cy=corner<2?y+h-r:y+r;
        pts[n++]=PointF(cx+copysignf(r*sqrtf(fabsf(cs)),cs),cy+copysignf(r*sqrtf(fabsf(sn)),sn));
    }p.AddPolygon(pts,n);
}
extern "C" void glassMaskPixels(void *pixels,int w,int h,int dpi){
    auto data=(unsigned char*)pixels;float scale=dpi/96.f;
    /* DIB byte order is BGRA; these are the neutral edge ink and the accent
       that lights the inner rim of the window outline. */
    const float ink[3]={55.f,42.f,31.f};
    const unsigned char *a=accentRgb();
    const float accent[3]={(float)a[2],(float)a[1],(float)a[0]};
    auto inside=[&](float d,float offset){float v=.5f-(d+offset*scale);return v<0.f?0.f:(v>1.f?1.f:v);};
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        float d=roundedDistance(x+.5f,y+.5f,(float)w,(float)h,(float)UI_R_PANEL*scale);
        float coverage=std::clamp(.5f-d,0.f,1.f);
        float stroke=inside(d,0.f)-inside(d,.8f);
        float rim=inside(d,1.3f)-inside(d,2.6f);
        auto px=data+(y*w+x)*4;int alpha=(int)lroundf(coverage*255);
        for(int c=0;c<3;c++){
            float value=px[c]+(ink[c]-px[c])*stroke*.26f;
            value+=(accent[c]-value)*rim*.13f;
            px[c]=(unsigned char)std::clamp(value*alpha/255,0.f,255.f);}px[3]=(unsigned char)alpha;
    }
}
static void drawLens(Graphics &g,float x,float y,float w,float h,float visibility,int dpi,Bitmap &surface){
    if(visibility<.01f)return;
    int nw=(int)ceilf(w*dpi/96.f),nh=(int)ceilf(h*dpi/96.f);
    Bitmap lens(nw,nh,PixelFormat32bppARGB);BitmapData out,in;Rect rr(0,0,nw,nh),sr(0,0,surface.GetWidth(),surface.GetHeight());
    if(surface.LockBits(&sr,ImageLockModeRead,PixelFormat32bppARGB,&in)!=Ok)return;
    if(lens.LockBits(&rr,ImageLockModeWrite,PixelFormat32bppARGB,&out)!=Ok){surface.UnlockBits(&in);return;}
    float scale=dpi/96.f;
    for(int py=0;py<nh;py++)for(int px=0;px<nw;px++){
        float u=(px+.5f)/scale,v=(py+.5f)/scale,d=roundedDistance(u,v,w,h,16);
        float coverage=std::clamp(.5f-d*scale,0.f,1.f);
        auto dest=(unsigned char*)out.Scan0+py*out.Stride+px*4;
        if(coverage==0){memset(dest,0,4);continue;}
        float nx=(roundedDistance(u+.3f,v,w,h,16)-roundedDistance(u-.3f,v,w,h,16))/.6f;
        float ny=(roundedDistance(u,v+.3f,w,h,16)-roundedDistance(u,v-.3f,w,h,16))/.6f;
        float edge=expf(-std::max(0.f,-d)/6.f),bend=12.f*edge*visibility;
        float fx=std::clamp(x+u-nx*bend,0.f,(float)sr.Width-1.001f),fy=std::clamp(y+v-ny*bend,0.f,(float)sr.Height-1.001f);
        int ix=(int)fx,iy=(int)fy;float ax=fx-ix,ay=fy-iy;
        float facing=std::clamp(-nx*.25f-ny*.75f,0.f,1.f);
        float spec=powf(std::max(0.f,-nx*.55f-ny*.83f),4)*expf(-fabsf(d+1.1f)/1.3f)*74;
        float rimBand=expf(-fabsf(d+.35f)/.7f),rim=rimBand*(6.f+facing*20.f);
        float underside=expf(-fabsf(d+1.2f)) * std::max(0.f,ny)*7.f;
        float shade=std::max(0.f,nx*.35f+ny*.65f)*edge*12;
        for(int c=0;c<3;c++){
            auto row=(unsigned char*)in.Scan0+iy*in.Stride;auto next=row+in.Stride;
            float value=(row[ix*4+c]*(1-ax)+row[(ix+1)*4+c]*ax)*(1-ay)+(next[ix*4+c]*(1-ax)+next[(ix+1)*4+c]*ax)*ay;
            float tint=accentRgb()[2-c];
            float softTint=255.f-(255.f-tint)*.09f;
            dest[c]=(unsigned char)std::clamp(value*.90f+softTint*.07f+255.f*.03f+spec+rim+underside-shade,0.f,255.f);
        }
        dest[3]=(unsigned char)(coverage*visibility*255);
    }
    lens.UnlockBits(&out);surface.UnlockBits(&in);
    g.DrawImage(&lens,RectF(x,y,w,h));
    const unsigned char *a=accentRgb();
    Pen topGlint(Color((BYTE)(visibility*128),255,255,255),.9f),sideGlint(Color((BYTE)(visibility*54),255,255,255),.7f);
    Pen lowerRim(Color((BYTE)(visibility*58),a[0],a[1],a[2]),.75f);
    g.DrawLine(&topGlint,x+12.f,y+.7f,x+w-17.f,y+.7f);g.DrawLine(&sideGlint,x+.7f,y+16.f,x+.7f,y+h-19.f);g.DrawLine(&lowerRim,x+17.f,y+h-.7f,x+w-17.f,y+h-.7f);
}
/* A 1px divider that dissolves at both ends stops the panel reading as a stack
   of hard rectangles. Cheap enough to redraw every frame. */
static void fadeLine(Graphics &g,float x1,float x2,float y,int alpha){
    const int segments=20;float span=(x2-x1)/segments;
    for(int i=0;i<segments;i++){
        float p=(i+.5f)/segments,a=1.f-fabsf(p*2.f-1.f);
        Pen pen(Color((BYTE)std::clamp(alpha*a,0.f,255.f),203,213,225),1.f);
        g.DrawLine(&pen,x1+i*span,y-.5f,x1+(i+1)*span+.6f,y-.5f);
    }
}
/* Every card is the same size, so the layered drop shadow is rasterised once
   and blitted. Filling four rounded paths per card per frame dominated the
   repaint budget; this keeps the depth without the cost. */
static Bitmap *cardShadowBitmap(float tileW,float tileH,float radius){
    if(cardShadow&&cardShadowTileW==tileW&&cardShadowTileH==tileH)return cardShadow;
    delete cardShadow;cardShadow=nullptr;
    const float pad=5.f;
    int w=(int)ceilf(tileW+pad*2),h=(int)ceilf(tileH+pad*2);
    cardShadow=new Bitmap(w,h,PixelFormat32bppPARGB);
    if(cardShadow->GetLastStatus()!=Ok){delete cardShadow;cardShadow=nullptr;return nullptr;}
    Graphics s(cardShadow);s.SetSmoothingMode(SmoothingModeAntiAlias);
    for(int i=4;i>=1;i--){
        GraphicsPath shadow;rounded(shadow,pad-(float)i,pad-(float)i+1.5f,tileW+2.f*i,tileH+2.f*i,radius+(float)i);
        SolidBrush ink(Color(9,51,65,85));s.FillPath(&ink,&shadow);
    }
    s.Flush(FlushIntentionSync);
    cardShadowTileW=tileW;cardShadowTileH=tileH;
    return cardShadow;
}
extern "C" void glassPaint(HDC dc,int width,int height,int dpi,int hover,int selected,BOOL pinned,const WCHAR **names,void **icons,const BOOL *occupied){
    float scale=dpi/96.f,w=width/scale,h=height/scale,pitch=(w-24)/4,tileW=pitch-8;
    int visibleRows=std::clamp((int)((h-GLASS_GRID_TOP-GLASS_FOOTER_HEIGHT)/GLASS_ROW_PITCH),1,slotCount/4);
    const unsigned char *t=accentRgb();
    const float gridTop=(float)GLASS_GRID_TOP,rowPitch=(float)GLASS_ROW_PITCH,tileH=92.f;
    /* Background and cards are identical on every frame unless the theme,
       geometry, occupancy or the blurred backdrop changed, so they are
       rasterised once and reused. Rebuilding them per frame cost more than a
       60 Hz budget allows; the lens still re-samples the live backdrop. */
    unsigned signature=2166136261u;
    for(int i=0;i<slotCount;i++)signature=(signature^(occupied[i]?1u:0u))*16777619u;
    signature=(signature^(unsigned)visibleRows)*16777619u;
    if(backgroundDirty)cardLayerValid=false;
    int layerW=(int)ceilf(w),layerH=(int)ceilf(h);
    if(!cardLayerValid||cardLayerW!=layerW||cardLayerH!=layerH||cardLayerRows!=visibleRows||
       cardLayerTheme!=themeIndex||cardLayerSlots!=slotCount||cardLayerOffset!=rowOffset||cardLayerSignature!=signature){
        delete cardLayer;cardLayer=nullptr;
        Bitmap *built=new Bitmap(layerW,layerH,PixelFormat32bppARGB);
        if(built->GetLastStatus()==Ok){
            Graphics b(built);b.SetInterpolationMode(InterpolationModeHighQualityBicubic);b.SetSmoothingMode(SmoothingModeAntiAlias);
            if(backdrop)b.DrawImage(backdrop,RectF(0,0,w,h));else b.Clear(Color(255,246,248,252));
            // Frosted light surface: an almost opaque white veil, a whisper of the
            // accent, then a vertical sheen so the top edge reads as lit.
            /* The veil is deliberately close to opaque: it pins the canvas to a
               known light slate so white cards keep their contrast on any desktop,
               while 7% of the blurred backdrop still tints the surface. */
            SolidBrush veil(Color(236,250,251,255));b.FillRectangle(&veil,0.f,0.f,w,h);
            SolidBrush wash(Color(16,t[0],t[1],t[2]));b.FillRectangle(&wash,0.f,0.f,w,h);
            SolidBrush slateTint(Color(64,208,216,231));b.FillRectangle(&slateTint,0.f,0.f,w,h);
            LinearGradientBrush canvas(RectF(0.f,0.f,w,h),Color(44,255,255,255),Color(0,248,250,253),LinearGradientModeVertical);b.FillRectangle(&canvas,0.f,0.f,w,h);
            // Recessed chrome bands top and bottom frame the grid without a hard rule.
            LinearGradientBrush headBand(RectF(0.f,0.f,w,(float)GLASS_HEADER_HEIGHT),Color(104,255,255,255),Color(0,255,255,255),LinearGradientModeVertical);
            b.FillRectangle(&headBand,0.f,0.f,w,(float)GLASS_HEADER_HEIGHT);
            LinearGradientBrush footBand(RectF(0.f,h-GLASS_FOOTER_HEIGHT,w,(float)GLASS_FOOTER_HEIGHT),Color(0,220,227,238),Color(48,220,227,238),LinearGradientModeVertical);
            b.FillRectangle(&footBand,0.f,h-GLASS_FOOTER_HEIGHT,w,(float)GLASS_FOOTER_HEIGHT);
            /* Layered translucent rings stand in for a gaussian drop shadow. */
            Bitmap *shadowImage=cardShadowBitmap(tileW,tileH,(float)UI_R_CARD);
            auto softShadow=[&](float x,float y){
                if(shadowImage)b.DrawImage(shadowImage,RectF(x-5.f,y-5.f,tileW+10.f,tileH+10.f));
            };
            for(int i=0;i<slotCount;i++){
                int row=slotRow(i);if(!visibleSlot(i,visibleRows))continue;
                float x=12+(i%4)*pitch,y=gridTop+row*rowPitch;GraphicsPath tile;rounded(tile,x,y,tileW,tileH,(float)UI_R_CARD);
                if(i<2){
                    softShadow(x,y);
                    LinearGradientBrush fill(RectF(x,y,tileW,tileH),Color(70,t[0],t[1],t[2]),Color(22,t[0],t[1],t[2]),LinearGradientModeVertical);
                    b.FillPath(&fill,&tile);
                    Pen border(Color(112,t[0],t[1],t[2]),1.f);b.DrawPath(&border,&tile);
                }else if(occupied[i]){
                    softShadow(x,y);
                    LinearGradientBrush card(RectF(x,y,tileW,tileH),Color(255,255,255,255),Color(255,249,251,254),LinearGradientModeVertical);
                    b.FillPath(&card,&tile);
                    // Top-lit hairline: a vertical gradient pen keeps the card crisp
                    // at the top and soft at the bottom.
                    LinearGradientBrush edgeBrush(RectF(x,y,tileW,tileH),Color(255,203,213,226),Color(200,219,227,238),LinearGradientModeVertical);
                    Pen edge(&edgeBrush,1.f);b.DrawPath(&edge,&tile);
                }else{
                    SolidBrush empty(Color(44,255,255,255));b.FillPath(&empty,&tile);
                    Pen edge(Color(150,148,163,184),1.f);edge.SetDashStyle(DashStyleDash);b.DrawPath(&edge,&tile);
                }
                GraphicsPath gloss;rounded(gloss,x+1.f,y+1.f,tileW-2.f,tileH-2.f,(float)UI_R_CARD-1.f);
                Pen glossPen(Color(i<2?96:190,255,255,255),1.f);b.DrawPath(&glossPen,&gloss);
            }
            fadeLine(b,15.f,w-15.f,(float)GLASS_HEADER_HEIGHT,232);
            fadeLine(b,15.f,w-15.f,gridTop+rowPitch-6.f,196);
            fadeLine(b,15.f,w-15.f,h-GLASS_FOOTER_HEIGHT,196);
            b.Flush(FlushIntentionSync);
            cardLayer=built;
        }else delete built;
        cardLayerW=layerW;cardLayerH=layerH;cardLayerRows=visibleRows;cardLayerTheme=themeIndex;
        cardLayerSlots=slotCount;cardLayerOffset=rowOffset;cardLayerSignature=signature;
        cardLayerValid=cardLayer!=nullptr;
    }
    if(!cardLayer)return;
    Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.ScaleTransform(scale,scale);
    g.DrawImage(cardLayer,RectF(0,0,w,h));
    ULONGLONG now=GetTickCount64();float dt=lastFrame?std::min((now-lastFrame)/1000.f,.1f):.033f;lastFrame=now;
    if(!interactiveSlot(selected)||!visibleSlot(selected,visibleRows))selected=-1;
    bool inside=selected<0&&pointerX>=12&&pointerX<w-12&&pointerY>=gridTop&&pointerY<gridTop+visibleRows*rowPitch;
    if(inside){int row=(int)((pointerY-gridTop)/rowPitch),col=(int)((pointerX-12)/pitch);int slot=row==0?col:(row+rowOffset)*4+col;inside=interactiveSlot(slot);}
    float tx=lensX,ty=lensY;
    if(inside){
        int col=std::clamp((int)((pointerX-12)/pitch),0,3),row=std::clamp((int)((pointerY-gridTop)/rowPitch),0,visibleRows-1);
        float sx=12+col*pitch,sy=gridTop+row*rowPitch;
        // Pointer hit regions select a slot, never an arbitrary lens position.
        // Keep the existing time-based interpolation for natural slot transitions.
        tx=sx;ty=sy;
    }else if(selected>=0){tx=12+(selected%4)*pitch;ty=gridTop+slotRow(selected)*rowPitch;}
    // The accent outline already marks the destination. Move the lens quickly
    // and fade its displaced image so a second, trailing frame is not visible.
    float blend=1-expf(-dt*64),visible=(inside||selected>=0)?1.f:0.f;
    if(lensVisibility<.01f){lensX=tx;lensY=ty;}
    if(selected>=0){lensX=tx;lensY=ty;}
    else {lensX+=(tx-lensX)*blend;lensY+=(ty-lensY)*blend;}
    lensVisibility+=(visible-lensVisibility)*blend;
    if(fabsf(tx-lensX)<.05f)lensX=tx;if(fabsf(ty-lensY)<.05f)lensY=ty;
    if(fabsf(visible-lensVisibility)<.005f)lensVisibility=visible;
    animating=fabsf(tx-lensX)>.05f||fabsf(ty-lensY)>.05f||fabsf(visible-lensVisibility)>.005f;
    float lensTravel=hypotf(tx-lensX,ty-lensY);
    float drawnVisibility=lensVisibility*std::clamp(1.f-lensTravel/16.f,0.f,1.f);
    GraphicsState lensClip=g.Save();g.SetClip(RectF(4,gridTop-3,w-8,h-gridTop-GLASS_FOOTER_HEIGHT+4));const float glassInset=.5f;drawLens(g,lensX+glassInset,lensY+glassInset,tileW-glassInset*2,tileH-glassInset*2,drawnVisibility,dpi,*cardLayer);g.Restore(lensClip);backgroundDirty=false;
    // The glass lens moves between cards; a stationary accent outline identifies the actionable card.
    int activeSlot=selected>=0?selected:hover;
    if(interactiveSlot(activeSlot)){
        int activeRow=slotRow(activeSlot);
        if(visibleSlot(activeSlot,visibleRows)){
            float x=12+(activeSlot%4)*pitch,y=gridTop+activeRow*rowPitch;
            GraphicsState accentClip=g.Save();g.SetClip(RectF(6,gridTop-2,w-12,h-gridTop-GLASS_FOOTER_HEIGHT+4));
            // Soft outer halo, gentle accent tint, then a crisp 2px ring: the
            // three layers read as one lit card rather than a hard outline.
            GraphicsPath halo;rounded(halo,x-2.5f,y-2.5f,tileW+5.f,tileH+5.f,(float)UI_R_CARD+2.5f);
            SolidBrush haloFill(Color(24,t[0],t[1],t[2]));g.FillPath(&haloFill,&halo);
            GraphicsPath outline;rounded(outline,x+.5f,y+.5f,tileW-1.f,tileH-1.f,(float)UI_R_CARD);
            SolidBrush tint(Color(26,t[0],t[1],t[2]));g.FillPath(&tint,&outline);
            Pen border(Color(250,t[0],t[1],t[2]),2.2f);border.SetLineJoin(LineJoinRound);g.DrawPath(&border,&outline);
            g.Restore(accentClip);
        }
    }
    if(chromeDpi!=dpi){const WCHAR *ids[]={L"logo",L"pin",L"gear",L"close",L"plus"};for(int i=0;i<5;i++){delete chrome[i];chrome[i]=asset(ids[i],MulDiv(i==4?28:20,dpi,96));}const WCHAR *plugins[]={L"plugin-renamer",L"plugin-cursor",L"plugin-add"};for(int i=0;i<3;i++){delete pluginChrome[i];pluginChrome[i]=asset(plugins[i],MulDiv(64,dpi,96));}chromeDpi=dpi;}
    Font font(L"Microsoft YaHei UI",12.2f,FontStyleRegular,UnitPixel),title(L"Microsoft YaHei UI",15.5f,FontStyleBold,UnitPixel),micro(L"Microsoft YaHei UI",9.6f,FontStyleRegular,UnitPixel);
    SolidBrush headerInk(Color(255,31,42,55)),subInk(Color(255,100,116,139)),muted(Color(255,120,134,152));
    {
        GraphicsPath logoPlate;rounded(logoPlate,14.f,13.f,30.f,30.f,(float)UI_R_PLATE);
        SolidBrush plateFill(Color(30,t[0],t[1],t[2]));g.FillPath(&plateFill,&logoPlate);
        Pen plateEdge(Color(52,t[0],t[1],t[2]),1.f);g.DrawPath(&plateEdge,&logoPlate);
        if(chrome[0])g.DrawImage(chrome[0],RectF(19,18,20,20));
    }
    g.DrawString(L"轻启",-1,&title,PointF(54,6),&headerInk);g.DrawString(L"中键唤醒 · 即点即开",-1,&micro,PointF(55,29),&subInk);
    for(int i=0;i<3;i++){
        float x=w-84+i*27;GraphicsPath button;rounded(button,x,15,26,26,(float)UI_R_PILL);
        bool hot=pointerY>=12.f&&pointerY<44.f&&pointerX>=x&&pointerX<x+26.f,active=i==0&&pinned;
        if(active||hot){SolidBrush fill(Color(active?112:56,t[0],t[1],t[2]));g.FillPath(&fill,&button);
            if(active){Pen ring(Color(148,t[0],t[1],t[2]),1.f);g.DrawPath(&ring,&button);}}
        if(chrome[i+1])g.DrawImage(chrome[i+1],RectF(x+5.f,20,16,16));
    }
    StringFormat format;format.SetAlignment(StringAlignmentCenter);format.SetLineAlignment(StringAlignmentCenter);format.SetTrimming(StringTrimmingEllipsisWord);format.SetFormatFlags(StringFormatFlagsLineLimit);
    SolidBrush ink(Color(255,32,44,58)),emptyInk(Color(255,130,144,163));
    GraphicsState gridClip=g.Save();g.SetClip(RectF(6,gridTop-2,w-12,h-gridTop-GLASS_FOOTER_HEIGHT+4));
    auto drawFunctionIcon=[&](int slot,float x,float y,float size){
        int identity=names[slot]&&contains(names[slot],L"重命名")?0:names[slot]&&(contains(names[slot],L"鼠标")||contains(names[slot],L"指针"))?1:2;
        if(pluginChrome[identity]){g.DrawImage(pluginChrome[identity],RectF(x,y,size,size));return;}
        GraphicsState symbolState=g.Save();g.TranslateTransform(x,y);g.ScaleTransform(size/40.f,size/40.f);
        SolidBrush symbolInk(Color(255,t[0],t[1],t[2])),symbolFill(Color(26,t[0],t[1],t[2]));
        GraphicsPath plate;rounded(plate,0,0,40,40,(float)UI_R_PLATE);g.FillPath(&symbolFill,&plate);
        Pen stroke(Color(255,t[0],t[1],t[2]),2.2f);stroke.SetStartCap(LineCapRound);stroke.SetEndCap(LineCapRound);stroke.SetLineJoin(LineJoinRound);
        (void)slot;GraphicsPath body;rounded(body,9,15,22,18,4);g.DrawPath(&stroke,&body);
        g.DrawLine(&stroke,15.f,15.f,15.f,8.f);g.DrawLine(&stroke,25.f,15.f,25.f,8.f);g.DrawLine(&stroke,20.f,33.f,20.f,37.f);
        g.DrawLine(&stroke,16.f,24.f,24.f,24.f);g.DrawLine(&stroke,20.f,20.f,20.f,28.f);
        g.Restore(symbolState);
    };
    auto drawItem=[&](int i,float x,float y,bool lifted){
        if(i>=133)return; // Four plugins, fixed Control Panel and 128 application positions.
        if(y+tileH<gridTop||y>h-GLASS_FOOTER_HEIGHT)return;
        GraphicsState itemClip=g.Save();if(i>=GLASS_FUNCTION_SLOTS)g.SetClip(RectF(6,gridTop+rowPitch-2,w-12,h-gridTop-rowPitch-GLASS_FOOTER_HEIGHT+4),CombineModeIntersect);
        float size=lifted?44.f:42.f;
        if(lifted){GraphicsPath raised;rounded(raised,x-2,y-2,tileW+4,tileH+4,(float)UI_R_CARD+2);SolidBrush shadow(Color(40,57,68,88));g.FillPath(&shadow,&raised);SolidBrush raisedFill(Color(36,t[0],t[1],t[2]));g.FillPath(&raisedFill,&raised);Pen rim(Color(150,t[0],t[1],t[2]),1.1f);g.DrawPath(&rim,&raised);}
        if(i<GLASS_FUNCTION_SLOTS)drawFunctionIcon(i,x+(tileW-size)/2,y+7,size);
        else if(occupied[i]){if(icons[i])g.DrawImage((Bitmap*)icons[i],RectF(x+(tileW-size)/2,y+7,size,size));}
        else if(i>=GLASS_FUNCTION_SLOTS){
            float plateSize=32.f,px=x+(tileW-plateSize)/2,py=y+11.f;
            GraphicsPath plusPlate;rounded(plusPlate,px,py,plateSize,plateSize,(float)UI_R_PLATE);
            SolidBrush plusFill(Color(22,t[0],t[1],t[2]));g.FillPath(&plusFill,&plusPlate);
            Pen plusEdge(Color(64,t[0],t[1],t[2]),1.f);g.DrawPath(&plusEdge,&plusPlate);
            if(chrome[4])g.DrawImage(chrome[4],RectF(px+7.f,py+7.f,18,18));
        }
        if(lifted){g.Restore(itemClip);return;}
        const WCHAR *label=i<GLASS_FUNCTION_SLOTS?(names[i]?names[i]:L""):occupied[i]?names[i]:L"添加应用";
        std::wstring balanced=label;size_t count=balanced.size();bool chinese=count>=7&&count<=12;
        if(chinese)for(WCHAR c:balanced)if(c<0x3400||c>0x9fff){chinese=false;break;}
        if(chinese)balanced.insert((count+1)/2,1,L'\n');
        RectF labelRect(x+5,y+51,tileW-10,34);g.DrawString(balanced.c_str(),-1,&font,labelRect,&format,occupied[i]?&ink:&emptyInk);
        g.Restore(itemClip);
    };
    for(int i=0;i<slotCount;i++){
        int visibleRow=slotRow(i);if(!visibleSlot(i,visibleRows))continue;
        float baseX=12+(i%4)*pitch,baseY=gridTop+visibleRow*rowPitch;
        if(!iconsReady){iconX[i]=baseX;iconY[i]=baseY;}
        int slot=i;
        if(dragFrom>=0&&dragTo>=0){if(dragFrom<dragTo&&i>dragFrom&&i<=dragTo)slot=i-1;else if(dragFrom>dragTo&&i>=dragTo&&i<dragFrom)slot=i+1;}
        float goalX=12+(slot%4)*pitch,goalY=gridTop+slotRow(slot)*rowPitch;
        if(i==dragFrom){goalX=pointerX-tileW/2;goalY=pointerY-tileH/2;iconX[i]=goalX;iconY[i]=goalY;}
        else{iconX[i]+=(goalX-iconX[i])*blend;iconY[i]+=(goalY-iconY[i])*blend;}
        if(fabsf(goalX-iconX[i])>.05f||fabsf(goalY-iconY[i])>.05f)animating=true;
        if(i!=dragFrom)drawItem(i,iconX[i],iconY[i],false);
    }
    iconsReady=true;
    if(dragFrom>=0)drawItem(dragFrom,iconX[dragFrom],iconY[dragFrom],true);
    g.Restore(gridClip);
    // Slim scroll affordance, drawn in the margin the tiles never occupy.
    if(slotCount/4>visibleRows){
        float trackX=w-8.f,trackTop=gridTop+2.f,trackHeight=h-GLASS_FOOTER_HEIGHT-gridTop-4.f;
        GraphicsPath track;rounded(track,trackX,trackTop,3.f,trackHeight,1.5f);
        SolidBrush trackFill(Color(64,203,213,225));g.FillPath(&trackFill,&track);
        float ratio=(float)visibleRows/(float)(slotCount/4),thumbHeight=std::max(24.f,trackHeight*ratio);
        float thumbTop=trackTop+(trackHeight-thumbHeight)*((float)rowOffset/std::max(1,slotCount/4-visibleRows));
        GraphicsPath thumb;rounded(thumb,trackX,thumbTop,3.f,thumbHeight,1.5f);
        SolidBrush thumbFill(Color(170,t[0],t[1],t[2]));g.FillPath(&thumbFill,&thumb);
    }
    int appCount=0;for(int i=GLASS_FUNCTION_SLOTS;i<slotCount;i++)if(occupied[i])appCount++;
    Font footerFont(L"Microsoft YaHei UI",10.2f,FontStyleRegular,UnitPixel);SolidBrush footerInk(Color(255,109,124,143));
    const WCHAR *hint=L"拖入添加 · 右键管理";
    if(activeSlot>=0&&activeSlot<GLASS_FUNCTION_SLOTS)hint=occupied[activeSlot]?names[activeSlot]:L"添加插件";else if(activeSlot==GLASS_FUNCTION_SLOTS)hint=L"打开控制面板";
    if(pointerY>=9.f&&pointerY<GLASS_HEADER_HEIGHT){if(pointerX>=w-30)hint=L"隐藏面板，仍在托盘运行";else if(pointerX>=w-57)hint=L"打开设置和管理菜单";else if(pointerX>=w-84)hint=pinned?L"取消固定面板":L"固定面板，失焦后仍显示";}
    g.DrawString(hint,-1,&footerFont,PointF(16,h-27),&footerInk);
    WCHAR count[64];if(slotCount/4>visibleRows)swprintf(count,64,L"应用%d–%d/%d排 · 滚轮",rowOffset+1,rowOffset+visibleRows-1,slotCount/4-1);else swprintf(count,64,L"%d个应用",appCount);
    {
        GraphicsPath pill;rounded(pill,w-176,h-31,160,22,11);
        SolidBrush pillFill(Color(205,241,245,249));g.FillPath(&pillFill,&pill);
        Pen pillEdge(Color(140,226,232,240),1.f);g.DrawPath(&pillEdge,&pill);
    }
    StringFormat right;right.SetAlignment(StringAlignmentFar);right.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(count,-1,&footerFont,RectF(w-176,h-31,150,22),&right,&muted);
}
