#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <tlhelp32.h>
#include <wchar.h>

/* Called only after confirmation in the visible installer. No termination,
   user-data writes, process-name killing or global --quit invocation. */
#define LL_MAX_PROCESSES 32
#define LL_PATH_CHARS 32768
#define LL_EXIT_MESSAGE (WM_APP+7)
#define LL_CLASS L"LiteLaunch.Native.Panel.v1"
enum { LL_OK=0, LL_INSPECT=-1, LL_PATH=-2, LL_BUSY=-3, LL_TIMEOUT=-4, LL_STARTING=-5 };
typedef struct {HANDLE handles[LL_MAX_PROCESSES];DWORD pids[LL_MAX_PROCESSES];int count;} LLProcesses;

static BOOL canonicalPath(LPCWSTR input,WCHAR *output){
    HANDLE file=CreateFileW(input,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE)return FALSE;
    DWORD size=GetFinalPathNameByHandleW(file,output,LL_PATH_CHARS,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    CloseHandle(file);return size&&size<LL_PATH_CHARS;
}
static void releaseProcesses(LLProcesses *list){for(int i=0;i<list->count;i++)CloseHandle(list->handles[i]);list->count=0;}
static int collectProcesses(LPCWSTR target,LLProcesses *list){
    list->count=0;if(!target||!target[0])return LL_PATH;
    DWORD attributes=GetFileAttributesW(target);
    if(attributes==INVALID_FILE_ATTRIBUTES){DWORD error=GetLastError();return error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND?LL_OK:LL_PATH;}
    WCHAR expected[LL_PATH_CHARS];if(!canonicalPath(target,expected))return LL_PATH;
    LPCWSTR leaf=wcsrchr(expected,L'\\');if(!leaf||_wcsicmp(leaf+1,L"LiteLaunch.exe"))return LL_PATH;
    DWORD session=0;if(!ProcessIdToSessionId(GetCurrentProcessId(),&session))return LL_INSPECT;
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return LL_INSPECT;
    PROCESSENTRY32W entry={0};entry.dwSize=sizeof(entry);BOOL more=Process32FirstW(snapshot,&entry);int status=LL_OK;
    if(!more){CloseHandle(snapshot);return LL_INSPECT;}
    do{
        if(_wcsicmp(entry.szExeFile,L"LiteLaunch.exe")||entry.th32ProcessID==GetCurrentProcessId())continue;
        DWORD otherSession=0;if(!ProcessIdToSessionId(entry.th32ProcessID,&otherSession)||session!=otherSession)continue;
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,entry.th32ProcessID);
        if(!process){if(GetLastError()!=ERROR_INVALID_PARAMETER)status=LL_INSPECT;continue;}
        if(WaitForSingleObject(process,0)==WAIT_OBJECT_0){CloseHandle(process);continue;}
        WCHAR image[LL_PATH_CHARS],actual[LL_PATH_CHARS];DWORD size=LL_PATH_CHARS;
        if(!QueryFullProcessImageNameW(process,0,image,&size)||!canonicalPath(image,actual)){
            if(WaitForSingleObject(process,0)!=WAIT_OBJECT_0)status=LL_INSPECT;
            CloseHandle(process);continue;
        }
        if(_wcsicmp(expected,actual)){CloseHandle(process);continue;}
        if(list->count==LL_MAX_PROCESSES){CloseHandle(process);status=LL_INSPECT;break;}
        list->handles[list->count]=process;list->pids[list->count++]=entry.th32ProcessID;
    }while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);if(status!=LL_OK)releaseProcesses(list);return status;
}
typedef struct {DWORD pid;HWND window;} LLWindow;
static BOOL CALLBACK findPanel(HWND window,LPARAM parameter){
    LLWindow *match=(LLWindow*)parameter;DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    if(pid==match->pid){WCHAR name[128];if(GetClassNameW(window,name,128)&&!wcscmp(name,LL_CLASS)){match->window=window;return FALSE;}}
    return TRUE;
}
static DWORD remainingTime(ULONGLONG deadline){ULONGLONG now=GetTickCount64();return now>=deadline?0:(DWORD)(deadline-now);}
__declspec(dllexport) int __cdecl LLProbe(LPCWSTR target){
    LLProcesses list;int status=collectProcesses(target,&list);if(status!=LL_OK)return status;
    int count=list.count;releaseProcesses(&list);return count;
}
__declspec(dllexport) int __cdecl LLClose(LPCWSTR target,DWORD timeout){
    if(timeout<100||timeout>30000)return LL_PATH;
    LLProcesses list;int status=collectProcesses(target,&list);if(status!=LL_OK)return status;
    ULONGLONG deadline=GetTickCount64()+timeout;
    for(int i=0;i<list.count&&status==LL_OK;i++){
        if(WaitForSingleObject(list.handles[i],0)==WAIT_OBJECT_0)continue;
        LLWindow match={list.pids[i],NULL};EnumWindows(findPanel,(LPARAM)&match);
        if(!match.window){status=LL_STARTING;break;}
        DWORD left=remainingTime(deadline);if(!left){status=LL_TIMEOUT;break;}
        DWORD_PTR response=0;SetLastError(ERROR_SUCCESS);
        if(!SendMessageTimeoutW(match.window,LL_EXIT_MESSAGE,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,left<1500?left:1500,&response)){
            if(WaitForSingleObject(list.handles[i],0)==WAIT_OBJECT_0)continue;
            status=GetLastError()==ERROR_ACCESS_DENIED?LL_INSPECT:LL_TIMEOUT;break;
        }
        /* Old clients return zero even when they accepted the exit. A destroyed
           window is not enough: wait on the retained process handle. */
        if(IsWindow(match.window)&&WaitForSingleObject(list.handles[i],0)!=WAIT_OBJECT_0){status=LL_BUSY;break;}
        left=remainingTime(deadline);
        if(WaitForSingleObject(list.handles[i],left)!=WAIT_OBJECT_0){status=LL_TIMEOUT;break;}
    }
    releaseProcesses(&list);
    if(status==LL_OK){int left=LLProbe(target);if(left!=0)status=left<0?left:LL_STARTING;}
    return status;
}
