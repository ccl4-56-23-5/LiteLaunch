#include "../glass.cpp"
#include <fstream>
struct IconImage {int size;std::vector<BYTE> bytes;};
static void u16(std::ofstream &f,unsigned n){char b[2]={(char)n,(char)(n>>8)};f.write(b,2);}
static void u32(std::ofstream &f,unsigned n){u16(f,n);u16(f,n>>16);}
int wmain(int argc,wchar_t **argv){
 if(argc!=4)return 1;glassInit();CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
 std::vector<IconImage> frames;
 for(int size:{16,24,32,48,64,128,256}){
  Bitmap *bitmap=readSvg(argv[1],size);if(!bitmap)return 2;
  if(size==256&&bitmap->Save(argv[3],&png,nullptr)!=Ok)return 3;
  IStream *stream=nullptr;CreateStreamOnHGlobal(nullptr,TRUE,&stream);if(!stream||bitmap->Save(stream,&png,nullptr)!=Ok)return 4;delete bitmap;
  STATSTG stat={};stream->Stat(&stat,STATFLAG_NONAME);HGLOBAL memory=nullptr;GetHGlobalFromStream(stream,&memory);
  BYTE *data=(BYTE*)GlobalLock(memory);IconImage image;image.size=size;image.bytes.assign(data,data+stat.cbSize.LowPart);GlobalUnlock(memory);stream->Release();frames.push_back(image);
 }
 std::ofstream file(argv[2],std::ios::binary);u16(file,0);u16(file,1);u16(file,(unsigned)frames.size());unsigned offset=6+16*(unsigned)frames.size();
 for(auto &frame:frames){char size=(char)(frame.size==256?0:frame.size);file.put(size);file.put(size);file.put(0);file.put(0);u16(file,1);u16(file,32);u32(file,(unsigned)frame.bytes.size());u32(file,offset);offset+=(unsigned)frame.bytes.size();}
 for(auto &frame:frames)file.write((const char*)frame.bytes.data(),frame.bytes.size());glassShutdown();return file.good()?0:5;
}
