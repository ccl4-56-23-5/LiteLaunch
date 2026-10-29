#include "ever_rules.h"
#include "ever_regex.h"
#include <cstdio>
#include <vector>
static int count=0;
static void check(bool ok,const char*name){++count;std::printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)ExitProcess(2);}
static std::wstring run(const ever::Rule&r,const std::wstring&name,size_t ordinal=0,size_t total=1,const std::wstring&path=L"",const std::wstring&original=L""){
    ever::Input in;in.currentName=name;in.originalName=original;in.path=path;in.ordinal=ordinal;in.totalRows=total;std::wstring result,error;
    if(!ever::apply(r,in,result,error)){std::fwprintf(stderr,L"Rule error: %ls\n",error.c_str());ExitProcess(3);}return result;
}
static void create(const std::wstring&path,const std::vector<BYTE>&bytes){HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);check(h!=INVALID_HANDLE_VALUE,"create owned read-only input fixture");DWORD n=0;check(bytes.empty()||(WriteFile(h,bytes.data(),(DWORD)bytes.size(),&n,nullptr)&&n==bytes.size()),"fixture content written");CloseHandle(h);}
static std::vector<BYTE> utf8(const std::wstring&s){int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);std::vector<BYTE>r(n);WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),(char*)r.data(),n,nullptr,nullptr);return r;}
int wmain(int argc,wchar_t**argv){
    if(argc<2){std::printf("Usage: ever_rules_test.exe <owned EverRegex.exe>\n");return 1;}
    ever::regexTestExecutable(argv[1]);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    using namespace ever;Rule r;r.text=L"X";r.position=Position::Index;
    check(run(r,L"abcd")==L"Xabcd","position zero is prefix");r.index=1;check(run(r,L"abcd")==L"Xabcd","position one is prefix");
    r.index=-1;check(run(r,L"abcd")==L"abcdX","position minus one is suffix");r.index=-2;check(run(r,L"abcd")==L"abcXd","position minus two inserts before final character");
    r.operation=Operation::Delete;r.index=-1;r.count=2;check(run(r,L"abcd")==L"ab","negative deletion removes two tail characters");r.index=-2;check(run(r,L"abcd")==L"ad","negative minus two deletion matches original formula");
    r=Rule();r.source=AddSource::Sequence;check(run(r,L"name",0,2)==L"1_name","increment padding uses count plus numeric start");
    r.sequenceStart=L"0001";check(run(r,L"name",0,2)==L"1_name","increment does not treat input zero count as padding width");
    r.sequenceStart=L"01";check(run(r,L"name",0,100)==L"001_name","increment aligns large batch to calculated width");
    r.mode=Mode::NumberArrange;check(run(r,L"name",0,2)==L"01","number arrange distinct extra zero branch");
    r.sequenceStart=L"1.5";check(run(r,L"name",1,2)==L"2.5","original Decimal sequence supports fractional start");
    r=Rule();r.source=AddSource::PinyinInitials;r.replaceEntireName=true;check(run(r,L"北京ABC")==L"bjabc","original pinyin ordered table then lowercase");
    r.pinyinCount=2;check(run(r,L"北京ABC")==L"bj","pinyin prefix counts all leading UTF16 characters");r.pinyinCount=-2;check(run(r,L"北京ABC")==L"bjabc","pinyin nonpositive means complete string");
    r.pinyinCount=0;check(run(r,L"G X")==L"gx","original multichar G-space rule preserved");
    check(run(r,L"廣")==L"a","original table result preserved even unusual pronunciation");
    r=Rule();r.source=AddSource::ChineseNumbers;r.frontAnchor=L"第";r.backAnchor=L"章";
    check(run(r,L"第十二章")==L"12第十二章","Chinese numeral default adds number at prefix");
    r.replaceOriginalNumbers=true;check(run(r,L"第十二章")==L"第12章","Chinese numeral only replaces regex-located region");
    r.frontAnchor=L"";r.backAnchor=L"";check(run(r,L"一万二千三百四十五点四五")==L"12345.45","Chinese numeric unit and decimal conversion");
    r=Rule();r.mode=Mode::FindReplace;r.replacements=L"(?<=第)(?<n>\\d+)|${n}_新";
    check(run(r,L"第12章")==L"第12_新章","actual .NET named capture and lookbehind semantics");
    r.regex=false;r.replacements=L"a|$1";check(run(r,L"aA")==L"$1$1","literal replacement keeps dollar group text literal");
    r.replacements=L"a|||A|B";check(run(r,L"a")==L"A|B","triple pipe delimiter preserves replacement pipe");
    r.replacements=L"|=@@\na@@x\nx@@y";check(run(r,L"a")==L"y","custom delimiter and sequential replacement order");
    r.regex=true;r.singleline=true;r.multiline=false;r.replacements=L"a.*b|x";check(run(r,L"a\r\nb")==L"x","singleline permits regex over file rows");
    r.singleline=false;r.multiline=true;r.replacements=L"^a|x";check(run(r,L"a\r\na")==L"x\r\nx","multiline anchors operate on whole multiline text");
    r.mode=Mode::PartialReplace;r.replacements=L"old|new";check(run(r,L"edited",0,1,L"",L"old\r\nold")==L"new\r\nnew","original partial branch processes joined original names instead of current edits");
    std::wstring response,error;int matchStart=0,matchLength=0;
    check(regexFind(L"(?<=第)\\d+",L"第12章 第34章",4,false,false,false,matchStart,matchLength,error)&&matchStart==6&&matchLength==2,"regex find UTF16 start and lookbehind");
    check(regexFind(L"missing",L"abc",0,false,false,false,matchStart,matchLength,error)&&matchStart==-1&&matchLength==0,"regex find no-match result");
    SYSTEMTIME time={};time.wYear=2025;time.wMonth=6;time.wDay=7;time.wHour=8;time.wMinute=9;time.wSecond=10;time.wMilliseconds=123;
    check(dotnetFormatTime(time,L"yyyyMMdd_HHmmss.fff",response,error)&&response==L"20250607_080910.123","full .NET custom datetime formats");
    check(dotnetFormatTime(time,L"MM-mm",response,error)&&response==L"06-09","original month versus minute semantics");
    r=Rule();r.pinyinCount=4;r.frontAnchor=L"第";r.backAnchor=L"章";r.replacements=L"\\n|新\r\n\\x|\\\\";r.escapeFind=true;r.escapeReplacement=false;
    Rule custom;custom.mode=Mode::NamingTemplate;custom.sequenceStart=L"1";custom.sequenceWidth=3;custom.namingPattern=L"xxxx-{num}";
    check(run(custom,L"photo",0,2)==L"xxxx-001","custom numbering first padded stem");
    custom.sequenceStart=L"98";custom.sequenceStep=2;check(run(custom,L"photo",2,3)==L"xxxx-102","number start step and width overflow expand");
    custom.namingPattern=L"{name}-{num}";check(run(custom,L"中文😀",0)==L"中文😀-098","Unicode current-name template");
    custom.namingPattern=L"{{name}}-{num}";check(run(custom,L"photo")==L"{name}-098","escaped braces remain literal");
    Input badInput;badInput.currentName=L"photo";std::wstring ignored;
    custom.namingPattern=L"{unknown}";check(!apply(custom,badInput,ignored,error),"unknown token fails clearly");
    custom.namingPattern=L"{name";check(!apply(custom,badInput,ignored,error),"unbalanced format fails clearly");
    custom.namingPattern=L"bad/name";check(!apply(custom,badInput,ignored,error),"illegal filename template rejected");
    custom.namingPattern=L"{num}";custom.sequenceStart=L"9223372036854775807";badInput.ordinal=1;
    check(!apply(custom,badInput,ignored,error),"sequence overflow rejected without wrapping");
    custom.sequenceStart=L"-1";check(!apply(custom,badInput,ignored,error),"negative start rejected");
    custom.sequenceStart=L"1";custom.namingPattern=L"{name}-{num}";
    Rule customRestored;check(deserialize(serialize(custom),customRestored,error)&&customRestored.mode==Mode::NamingTemplate&&customRestored.namingPattern==custom.namingPattern,"custom template persistence roundtrip");
    std::wstring oldSerialized=serialize(r);size_t patternAt=oldSerialized.find(L"namingPattern=");oldSerialized.erase(patternAt,oldSerialized.find(L'\n',patternAt)-patternAt+1);
    Rule oldRule;check(deserialize(oldSerialized,oldRule,error),"old V1 templates without new pattern field remain readable");
    Rule restored;check(deserialize(serialize(r),restored,error)&&serialize(r)==serialize(restored),"all rule fields and escaped template content roundtrip");
    check(!deserialize(serialize(r)+L"mode=1\n",restored,error),"duplicate template fields rejected");
    wchar_t temp[MAX_PATH]={};GetTempPathW(MAX_PATH,temp);std::wstring base=std::wstring(temp)+L"EverRules-QA-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    check(CreateDirectoryW(base.c_str(),nullptr)!=FALSE,"unique fixture folder");std::wstring txt=base+L"\\body.txt";create(txt,utf8(L"  甲\\乙/丙:丁?戊\"己<庚>辛*壬|癸 子丑寅卯\r\n末尾"));
    r=Rule();r.mode=Mode::ExtractBeginning;check(run(r,L"edited",0,1,txt,L"original")==L"甲乙丙丁戊己庚辛壬癸子丑","default twelve filtered body characters");
    r.extractLength=3;check(run(r,L"edited",0,1,txt,L"original")==L"甲乙丙","configured extracted length");
    std::wstring boundary=base+L"\\boundary.txt";std::vector<BYTE>bytes(3145728,'a');create(boundary,bytes);
    check(run(r,L"edited",0,1,boundary,L"original")==L"original","strict three megabyte boundary preserves original name");
    std::wstring prefix=base+L"\\prefix.txt";create(prefix,utf8(std::wstring(1000,L' ')+L"不可读取到此处"));
    Input in;in.path=prefix;in.currentName=L"initial";check(!apply(r,in,response,error),"first thousand character extraction boundary enforced");
    r=Rule();r.source=AddSource::ParentName;check(run(r,L"name",0,1,txt)==base.substr(base.find_last_of(L'\\')+1)+L"_name","parent directory connector prefix");
    r.position=Position::Suffix;check(run(r,L"name",0,1,txt)==L"name_"+base.substr(base.find_last_of(L'\\')+1),"parent directory connector suffix");
    r=Rule();r.source=AddSource::CreatedTime;r.replaceEntireName=true;r.timeFormat=L"yyyyMMdd";check(run(r,L"name",0,1,txt).size()==8,"read real creation date");
    r.source=AddSource::ModifiedTime;check(run(r,L"name",0,1,txt).size()==8,"read real update date");r.source=AddSource::CurrentTime;check(run(r,L"name").size()==8,"local current date");
    r.source=AddSource::PhotoTime;in.path=txt;check(!apply(r,in,response,error),"missing photo metadata reports failure");
    if(argc>2&&argv[2][0]){
        std::wstring fixtures=argv[2];r=Rule();r.mode=Mode::ExtractBeginning;r.extractLength=3;
        check(run(r,L"docx",0,1,fixtures+L"\\body.docx")==L"甲乙丙","actual Word DOCX read-only body extraction");
        check(run(r,L"doc",0,1,fixtures+L"\\body.doc")==L"甲乙丙","actual Word DOC read-only body extraction");
        r=Rule();r.source=AddSource::PhotoTime;r.replaceEntireName=true;r.timeFormat=L"yyyyMMdd_HHmmss";
        check(run(r,L"photo",0,1,fixtures+L"\\photo.jpg")==L"20200102_030405","actual EXIF DateTimeOriginal metadata extraction and local clock");
    }
    custom.sequenceStart=L"7";custom.sequenceWidth=3;custom.sequenceStep=1;custom.namingPattern=L"{created:yyyyMMdd}-{modified:yyyyMMdd}-{num}";
    check(run(custom,L"photo",0,1,txt).size()==21,"creation and modification dates combine with sequence");
    custom.namingPattern=L"{taken:yyyyMMdd_HHmmss}-{num}";in.path=txt;
    check(!apply(custom,in,response,error),"missing EXIF never silently substitutes another timestamp");
    if(argc>3)check(run(custom,L"photo",0,1,argv[3])==L"20200102_030405-007","actual JPEG EXIF timestamp combined with custom number");
    check(!regexReplace(L"[",L"",L"a",true,false,false,false,false,false,response,error),"invalid .NET expression reports clear error");
    ULONGLONG started=GetTickCount64();check(!regexReplace(L"(a+)+$",L"x",std::wstring(2000,L'a')+L"!",true,false,false,false,false,false,response,error)&&GetTickCount64()-started<7000,"actual .NET catastrophic regex timeout bounded");
    check(regexReplace(L"a",L"b",L"a",true,false,false,false,false,false,response,error)&&response==L"b","worker remains usable after regex timeout");
    check(!regexReplace(L"a",L"b",std::wstring(1,(wchar_t)0xD800),true,false,false,false,false,false,response,error),"invalid UTF16 returns error without escaping public bridge");
    check(regexReplace(L"a",L"b",L"a",true,false,false,false,false,false,response,error)&&response==L"b","worker restarts safely after input protocol exception");
    regexShutdown();DeleteFileW(txt.c_str());DeleteFileW(boundary.c_str());DeleteFileW(prefix.c_str());check(RemoveDirectoryW(base.c_str())!=FALSE,"only own fixture files cleaned");CoUninitialize();std::printf("ALL PASS %d checks\n",count);return 0;
}
