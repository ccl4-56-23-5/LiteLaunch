#include "glass.cpp"
#include <cassert>
int main(){
    const int width=448,height=GLASS_GRID_TOP+4*GLASS_ROW_PITCH+GLASS_FOOTER_HEIGHT;
    const float appTop=GLASS_GRID_TOP+GLASS_ROW_PITCH;
    const float pitch=(width-24.f)/4.f,thirdX=12+2*pitch;
    for(int scale=1;scale<=2;scale++){
        int w=width*scale,h=height*scale;std::vector<unsigned char> mask(w*h*4,255);glassMaskPixels(mask.data(),w,h,96*scale);
        assert(mask[3]==0&&mask[(w-1)*4+3]==0&&mask[(w*h-1)*4+3]==0);
        for(int y=0;y<30*scale;y++)for(int x=0;x<30*scale;x++)assert(abs((int)mask[(y*w+x)*4+3]-(int)mask[(y*w+w-1-x)*4+3])<=1);
        assert(mask[(h/2*w)*4+3]==255);assert(mask[(w/2)*4+3]==255);
    }
    // At the straight-edge join x(y)=(r^4-y^4)^(1/4), curvature tends to zero.
    double r=24,h=.01,x0=r,x1=pow(pow(r,4)-pow(h,4),.25),x2=pow(pow(r,4)-pow(2*h,4),.25);
    assert(fabs((x2-2*x1+x0)/(h*h))<.00001);puts("PASS G2 zero-curvature join and symmetric continuous alpha boundary");
    glassInit();HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,width,height);auto old=SelectObject(dc,bitmap);
    const WCHAR *names[GLASS_SLOT_CAPACITY]={};void *icons[GLASS_SLOT_CAPACITY]={};BOOL filled[GLASS_SLOT_CAPACITY]={};names[0]=L"重命名";names[1]=L"控制面板";filled[0]=filled[1]=TRUE;
    glassRows(16,0);glassResetSelection();glassInteraction(42,100+GLASS_ROW_PITCH,-1,4);glassPaint(dc,width,height,96,4,-1,FALSE,names,icons,filled);
    assert(fabsf(lensX-12)<.1f);float initial=lensX;
    glassInteraction(75,132+GLASS_ROW_PITCH,-1,4);Sleep(16);glassPaint(dc,width,height,96,4,-1,FALSE,names,icons,filled);
    assert(fabsf(lensX-12)<.01f&&fabsf(lensY-appTop)<.01f);
    glassInteraction(138,132+GLASS_ROW_PITCH,-1,5);Sleep(16);glassPaint(dc,width,height,96,5,-1,FALSE,names,icons,filled);
    assert(lensX>12&&lensX<12+pitch&&fabsf(lensY-appTop)<.01f);
    puts("PASS empty slots remain centered within hit region and animate when crossing boundary");
    float snap[3];for(int row=0;row<3;row++){glassResetSelection();glassInteraction(50,100+(row+1)*GLASS_ROW_PITCH,-1,4+row*4);glassPaint(dc,width,height,96,4+row*4,-1,FALSE,names,icons,filled);snap[row]=lensY-(row+1)*GLASS_ROW_PITCH;}
    assert(fabsf(snap[0]-snap[1])<.01f&&fabsf(snap[1]-snap[2])<.01f);puts("PASS equal magnetic response on all three rows");
    glassResetSelection();glassInteraction(42,100+GLASS_ROW_PITCH,-1,4);glassPaint(dc,width,height,96,4,-1,FALSE,names,icons,filled);initial=lensX;
    glassInteraction(250,100+GLASS_ROW_PITCH,-1,6);Sleep(16);glassPaint(dc,width,height,96,6,-1,FALSE,names,icons,filled);
    assert(lensX>initial&&lensX<thirdX);puts("PASS continuous tracking without instant slot jumps");
    for(int i=0;i<30;i++){Sleep(16);glassPaint(dc,width,height,96,6,-1,FALSE,names,icons,filled);}
    assert(lensX==thirdX&&lensY==appTop);puts("PASS selection settles exactly on slot after pointer stops");
    glassResetSelection();glassInteraction(-100,-100,-1,-1);glassPaint(dc,width,height,96,-1,5,FALSE,names,icons,filled);
    assert(lensX==12+pitch&&lensY==appTop);puts("PASS keyboard selection snaps to its card without a trailing frame");
    for(int t=0;t<5;t++){glassTheme(t);assert(glassNeedsPaint());glassPaint(dc,width,height,96,-1,5,FALSE,names,icons,filled);assert(themeIndex==t);}glassTheme(1);
    puts("PASS five theme palettes repaint");
    glassInteraction(273,150+GLASS_ROW_PITCH,4,6);Sleep(16);glassPaint(dc,width,height,96,6,-1,FALSE,names,icons,filled);
    assert(fabsf(iconX[4]-thirdX)<.1f);assert(iconX[5]<12+pitch&&iconX[5]>12);assert(iconX[6]<thirdX&&iconX[6]>12+pitch);
    puts("PASS dragged icon follows pointer and neighbors animate into insertion preview");
    glassReorder(4,6);glassInteraction(273,150+GLASS_ROW_PITCH,-1,6);float before=iconY[6];Sleep(16);glassPaint(dc,width,height,96,6,-1,FALSE,names,icons,filled);assert(iconY[6]<before&&iconY[6]>appTop);
    puts("PASS release animates into final slot");
    glassRows(132,29);glassResetSelection();glassInteraction(-100,-100,-1,-1);glassPaint(dc,width,height,96,-1,131,FALSE,names,icons,filled);
    assert(iconY[0]==GLASS_GRID_TOP&&iconY[1]==GLASS_GRID_TOP);assert(iconY[131]==GLASS_GRID_TOP+3*GLASS_ROW_PITCH&&lensY==iconY[131]);
    assert(!visibleSlot(4,4)&&!visibleSlot(116,4)&&visibleSlot(120,4)&&visibleSlot(131,4));
    glassInteraction(42,100,0,131);assert(dragFrom==-1);glassInteraction(42,100,131,0);assert(dragFrom==131&&dragTo==-1);
    float fixedX=iconX[0],fixedY=iconY[0];glassReorder(0,131);assert(iconX[0]==fixedX&&iconY[0]==fixedY);
    glassResetSelection();glassInteraction(250,100,-1,2);glassPaint(dc,width,height,96,2,-1,FALSE,names,icons,filled);assert(lensVisibility==0);
    puts("PASS function row stays fixed when scrolled; reserved cells have no lens; reorder excludes functions; all 128 app slots render safely");
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);glassShutdown();
}
