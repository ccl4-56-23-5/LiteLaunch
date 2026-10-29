#include "ever_regex.h"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace ever {
namespace {
struct Worker { HANDLE process=nullptr,input=nullptr,output=nullptr,job=nullptr; std::mutex mutex; std::wstring testPath; ~Worker(); } worker;
static void closeHandle(HANDLE &h) {if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);h=nullptr;}
static void stop() {
    closeHandle(worker.input);closeHandle(worker.output);
    if(worker.process){if(WaitForSingleObject(worker.process,2000)==WAIT_TIMEOUT){TerminateProcess(worker.process,1);WaitForSingleObject(worker.process,2000);}closeHandle(worker.process);}
    closeHandle(worker.job); // KILL_ON_JOB_CLOSE also reaps this worker's own metadata/script children.
}
Worker::~Worker(){stop();}
static bool fail(std::wstring&err,const wchar_t*message){err=message;return false;}
static std::string utf8(const std::wstring&s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);if(!n)throw std::runtime_error("UTF16");std::string r(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),&r[0],n,nullptr,nullptr);return r;}
static bool utf16(const std::string&s,std::wstring&out){if(s.empty()){out.clear();return true;}int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);if(!n)return false;out.resize(n);return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),&out[0],n)!=0;}
static bool executable(std::wstring&path,std::wstring&err){
#ifdef EVER_REGEX_TESTING
    if(!worker.testPath.empty()){path=worker.testPath;return true;}
#endif
    HRSRC resource=FindResourceW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(301),MAKEINTRESOURCEW(10));
    if(!resource)return fail(err,L"缺少内嵌.NET正则组件，请重新安装完整LiteLaunch版本。");
    DWORD size=SizeofResource(GetModuleHandleW(nullptr),resource);HGLOBAL loaded=LoadResource(GetModuleHandleW(nullptr),resource);const BYTE*bytes=(const BYTE*)LockResource(loaded);
    if(!bytes||!size)return fail(err,L"内嵌正则组件无效。");
    wchar_t base[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,base)))return fail(err,L"无法取得正则组件缓存目录。");
    std::wstring directory=std::wstring(base)+L"\\LiteLaunch";CreateDirectoryW(directory.c_str(),nullptr);directory+=L"\\runtime";CreateDirectoryW(directory.c_str(),nullptr);directory+=L"\\1.0.4";CreateDirectoryW(directory.c_str(),nullptr);
    path=directory+L"\\EverRegex.exe";bool equal=false;
    HANDLE previous=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(previous!=INVALID_HANDLE_VALUE){LARGE_INTEGER oldSize;DWORD got=0;if(GetFileSizeEx(previous,&oldSize)&&oldSize.QuadPart==size){std::vector<BYTE>old(size);equal=ReadFile(previous,old.data(),size,&got,nullptr)&&got==size&&!memcmp(old.data(),bytes,size);}CloseHandle(previous);}
    if(equal)return true;
    std::wstring temporary=path+L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
    HANDLE out=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(out==INVALID_HANDLE_VALUE)return fail(err,L"无法保存内嵌正则组件。");
    DWORD wrote=0;bool ok=WriteFile(out,bytes,size,&wrote,nullptr)&&wrote==size;FlushFileBuffers(out);CloseHandle(out);
    if(!ok||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());return fail(err,L"正则组件缓存写入失败。");}return true;
}
static bool start(std::wstring&err){if(worker.process&&WaitForSingleObject(worker.process,0)==WAIT_TIMEOUT)return true;stop();std::wstring path;if(!executable(path,err))return false;
    SECURITY_ATTRIBUTES security={sizeof(security),nullptr,TRUE};HANDLE childInput=nullptr,childOutput=nullptr,nullError=nullptr;
    if(!CreatePipe(&childInput,&worker.input,&security,65536)||!CreatePipe(&worker.output,&childOutput,&security,65536)){closeHandle(childInput);closeHandle(childOutput);stop();return fail(err,L"无法建立正则组件管道。");}
    SetHandleInformation(worker.input,HANDLE_FLAG_INHERIT,0);SetHandleInformation(worker.output,HANDLE_FLAG_INHERIT,0);
    nullError=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    STARTUPINFOW si={};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;si.hStdInput=childInput;si.hStdOutput=childOutput;si.hStdError=nullError;
    PROCESS_INFORMATION pi={};std::wstring command=L"\""+path+L"\"";
    worker.job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!worker.job||!SetInformationJobObject(worker.job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){closeHandle(childInput);closeHandle(childOutput);closeHandle(nullError);stop();return fail(err,L"无法建立规则组件进程管理。");}
    BOOL ok=CreateProcessW(path.c_str(),&command[0],nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&si,&pi);
    if(ok){if(!AssignProcessToJobObject(worker.job,pi.hProcess)){TerminateProcess(pi.hProcess,1);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);ok=FALSE;}else ResumeThread(pi.hThread);}
    closeHandle(childInput);closeHandle(childOutput);closeHandle(nullError);
    if(!ok){stop();return fail(err,L"无法启动内置.NET Framework正则组件。请检查Windows .NET Framework。");}worker.process=pi.hProcess;CloseHandle(pi.hThread);return true;
}
static bool write(const void*data,DWORD size){const BYTE*p=(const BYTE*)data;while(size){DWORD sent=0;if(!WriteFile(worker.input,p,std::min(size,(DWORD)4096),&sent,nullptr)||!sent)return false;p+=sent;size-=sent;}return true;}
static bool read(void*data,DWORD size,ULONGLONG deadline){BYTE*p=(BYTE*)data;while(size){DWORD available=0;if(!PeekNamedPipe(worker.output,nullptr,0,nullptr,&available,nullptr))return false;if(!available){if(GetTickCount64()>=deadline||WaitForSingleObject(worker.process,0)!=WAIT_TIMEOUT)return false;Sleep(10);continue;}DWORD got=0;if(!ReadFile(worker.output,p,std::min(size,available),&got,nullptr)||!got)return false;p+=got;size-=got;}return true;}
}
static bool request(int flags,const std::wstring&pattern,const std::wstring&replacement,const std::wstring&input,std::wstring&output,std::wstring&error){
    try {
    std::lock_guard<std::mutex>guard(worker.mutex);if(!start(error))return false;
    std::string parts[3]={utf8(pattern),utf8(replacement),utf8(input)};for(const auto&p:parts)if(p.size()>4*1024*1024)return fail(error,L"输入文本超过4MB。");
    bool ok=write(&flags,4);for(const auto&p:parts){DWORD size=(DWORD)p.size();ok=ok&&write(&size,4)&&write(p.data(),size);}
    int status=0;DWORD size=0;ULONGLONG deadline=GetTickCount64()+((flags&1024)?120000:15000);
    if(!ok||!read(&status,4,deadline)||!read(&size,4,deadline)||size>4*1024*1024){stop();return fail(error,L"正则组件响应失败或超时，操作已停止。");}
    std::string response(size,0);if(size&&!read(&response[0],size,deadline)){stop();return fail(error,L"正则组件响应不完整。");}
    std::wstring text;if(!utf16(response,text)){stop();return fail(error,L"正则组件返回了无效文本。");}if(!status){error=text;return false;}output=text;return true;
    }catch(const std::exception&){std::lock_guard<std::mutex>guard(worker.mutex);stop();return fail(error,L"输入Unicode文本或正则组件内存无效，操作已停止。");}
}
bool regexReplace(const std::wstring&pattern,const std::wstring&replacement,const std::wstring&input,bool regex,bool ignoreCase,bool singleline,bool multiline,bool escapeFind,bool escapeReplacement,std::wstring&output,std::wstring&error){
    return request((regex?1:0)|(ignoreCase?2:0)|(singleline?4:0)|(multiline?8:0)|(escapeFind?16:0)|(escapeReplacement?32:0),pattern,replacement,input,output,error);
}
bool regexFind(const std::wstring&pattern,const std::wstring&input,int startUtf16,bool ignoreCase,bool singleline,bool multiline,int&outStartUtf16,int&outLengthUtf16,std::wstring&error){
    std::wstring response;if(!request(65|(ignoreCase?2:0)|(singleline?4:0)|(multiline?8:0),pattern,std::to_wstring(startUtf16),input,response,error))return false;
    int position=0,length=0;if(swscanf(response.c_str(),L"%d\n%d",&position,&length)!=2)return fail(error,L"正则查找返回了无效位置。");outStartUtf16=position;outLengthUtf16=length;return true;
}
bool dotnetFormatTime(const SYSTEMTIME&t,const std::wstring&format,std::wstring&output,std::wstring&error){
    wchar_t iso[50]={};swprintf(iso,50,L"%04u-%02u-%02uT%02u:%02u:%02u.%03u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
    return request(256,format,L"",iso,output,error);
}
bool dotnetSequence(const std::wstring&start,size_t ordinal,size_t totalRows,long long step,int width,bool numberArrange,std::wstring&output,std::wstring&error){
    std::wstring parameters=std::to_wstring(ordinal)+L"|"+std::to_wstring(totalRows)+L"|"+std::to_wstring(step)+L"|"+(numberArrange?L"1":L"0")+L"|"+std::to_wstring(width);
    return request(512,start,parameters,L"",output,error);
}
void regexShutdown(){std::lock_guard<std::mutex>guard(worker.mutex);stop();}
bool advancedTransform(const std::wstring&settings,const std::wstring&rows,std::wstring&output,std::wstring&error){return request(1024,settings,L"",rows,output,error);}
#ifdef EVER_REGEX_TESTING
void regexTestExecutable(const std::wstring&path){std::lock_guard<std::mutex>guard(worker.mutex);stop();worker.testPath=path;}
#endif
}
