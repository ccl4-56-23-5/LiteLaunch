#include "glass.cpp"
int wmain(int argc,wchar_t **argv){if(argc!=3)return 1;glassInit();Bitmap *b=readSvg(argv[1],1024);if(!b)return 2;CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};Status result=b->Save(argv[2],&png,NULL);delete b;glassShutdown();return result==Ok?0:3;}
