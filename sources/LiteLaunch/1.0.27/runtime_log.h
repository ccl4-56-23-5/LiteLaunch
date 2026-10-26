#include <stdarg.h>
/* Small local UTF-16 diagnostic log. No application-list content is recorded. */
static void runtimeLog(LPCWSTR format,...) {
#ifndef LITELAUNCH_NO_RUNTIME_LOG
    WCHAR root[MAX_PATH],path[MAX_PATH],previous[MAX_PATH],message[1600],line[1800];
    if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,root)))return;
    if(wcslen(root)>MAX_PATH-48)return;
    lstrcatW(root,L"\\LiteLaunch");
    if(!CreateDirectoryW(root,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return;
    swprintf(path,MAX_PATH,L"%ls\\startup.log",root);
    WIN32_FILE_ATTRIBUTE_DATA data;
    if(GetFileAttributesExW(path,GetFileExInfoStandard,&data)&&(data.nFileSizeHigh||data.nFileSizeLow>524288)){
        swprintf(previous,MAX_PATH,L"%ls\\startup.previous.log",root);
        MoveFileExW(path,previous,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    }
    va_list args;va_start(args,format);_vsnwprintf(message,1599,format,args);va_end(args);message[1599]=0;
    SYSTEMTIME now;GetLocalTime(&now);
    swprintf(line,1800,L"%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu %ls\r\n",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,GetCurrentProcessId(),message);
    HANDLE file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return;
    LARGE_INTEGER size={0};DWORD written;GetFileSizeEx(file,&size);
    if(!size.QuadPart){WORD bom=0xfeff;WriteFile(file,&bom,sizeof(bom),&written,NULL);}
    WriteFile(file,line,(DWORD)(wcslen(line)*sizeof(WCHAR)),&written,NULL);CloseHandle(file);
#else
    (void)format;
#endif
}
