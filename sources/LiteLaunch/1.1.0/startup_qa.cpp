#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <taskschd.h>
#include <sddl.h>
#include <stdio.h>
#include <wchar.h>
#include "startup.h"

#ifndef LITELAUNCH_STARTUP_TESTING
#error Build this isolated QA executable with LITELAUNCH_STARTUP_TESTING defined.
#endif
template<class T> struct Pointer{T *p=nullptr;~Pointer(){if(p)p->Release();}T **out(){return &p;}T* operator->()const{return p;}};
struct String{BSTR p;explicit String(LPCWSTR s):p(SysAllocString(s)){}~String(){SysFreeString(p);}};
static WCHAR qaTask[256],qaKey[512],qaRoot[512],sid[192];
static int failures=0,checks=0;
static void check(bool ok,const char *label){checks++;printf("%s %s\n",ok?"PASS":"FAIL",label);if(!ok)failures++;}
static void apiCheck(BOOL ok,const char *label){check(ok!=FALSE,label);if(!ok)printf("HRESULT=0x%08lX\n",(unsigned long)startupLastError());}
static bool currentSid(){
    HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;
    DWORD bytes=0;GetTokenInformation(token,TokenUser,nullptr,0,&bytes);
    TOKEN_USER *user=(TOKEN_USER*)HeapAlloc(GetProcessHeap(),0,bytes);
    LPWSTR text=nullptr;bool ok=user&&GetTokenInformation(token,TokenUser,user,bytes,&bytes)&&ConvertSidToStringSidW(user->User.Sid,&text);
    if(ok)swprintf(sid,192,L"%ls",text);LocalFree(text);HeapFree(GetProcessHeap(),0,user);CloseHandle(token);return ok;
}
static bool folder(Pointer<ITaskService>& service,Pointer<ITaskFolder>& root){
    VARIANT empty;VariantInit(&empty);String path(L"\\");
    return SUCCEEDED(CoCreateInstance(CLSID_TaskScheduler,nullptr,CLSCTX_INPROC_SERVER,IID_ITaskService,(void**)service.out()))&&SUCCEEDED(service->Connect(empty,empty,empty,empty))&&SUCCEEDED(service->GetFolder(path.p,root.out()));
}
static bool getTask(ITaskFolder *root,Pointer<IRegisteredTask>& task){String name(qaTask);return SUCCEEDED(root->GetTask(name.p,task.out()));}
static bool setLegacy(LPCWSTR command){
    HKEY key=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,qaKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr))return false;
    LONG r=RegSetValueExW(key,L"LiteLaunch",0,REG_SZ,(const BYTE*)command,(DWORD)((wcslen(command)+1)*sizeof(WCHAR)));RegCloseKey(key);return r==ERROR_SUCCESS;
}
static bool legacyPresent(LPCWSTR name=L"LiteLaunch"){
    HKEY key=nullptr;if(RegOpenKeyExW(HKEY_CURRENT_USER,qaKey,0,KEY_QUERY_VALUE,&key))return false;
    LONG r=RegQueryValueExW(key,name,nullptr,nullptr,nullptr,nullptr);RegCloseKey(key);return r==ERROR_SUCCESS;
}
static bool rewrite(ITaskFolder *root,LPCWSTR executable,LPCWSTR author){
    Pointer<IRegisteredTask> old;Pointer<ITaskDefinition> definition;if(!getTask(root,old)||FAILED(old->get_Definition(definition.out())))return false;
    if(executable){Pointer<IActionCollection> actions;Pointer<IAction> action;Pointer<IExecAction> exec;String path(executable);
        if(FAILED(definition->get_Actions(actions.out()))||FAILED(actions->get_Item(1,action.out()))||FAILED(action->QueryInterface(IID_IExecAction,(void**)exec.out()))||FAILED(exec->put_Path(path.p)))return false;
    }
    if(author){Pointer<IRegistrationInfo> info;String text(author);if(FAILED(definition->get_RegistrationInfo(info.out()))||FAILED(info->put_Author(text.p)))return false;}
    String taskName(qaTask),userSid(sid);VARIANT user,empty;VariantInit(&user);VariantInit(&empty);user.vt=VT_BSTR;user.bstrVal=userSid.p;
    Pointer<IRegisteredTask> registered;return SUCCEEDED(root->RegisterTaskDefinition(taskName.p,definition.p,TASK_CREATE_OR_UPDATE,user,empty,TASK_LOGON_INTERACTIVE_TOKEN,empty,registered.out()));
}
static bool contains(BSTR xml,LPCWSTR expected){return xml&&wcsstr(xml,expected);}
static void verifyXml(ITaskFolder *root){
    Pointer<IRegisteredTask> task;check(getTask(root,task),"registered task can be queried");if(!task.p)return;
    BSTR xml=nullptr;check(SUCCEEDED(task->get_Xml(&xml)),"task XML can be read back");
    check(contains(xml,L"<LogonTrigger>")&&contains(xml,L"<Delay>PT20S</Delay>")&&contains(xml,sid),"current-user logon trigger and 20-second delay");
    Pointer<ITaskDefinition> definition;Pointer<IPrincipal> principal;Pointer<ITaskSettings> settings;
    bool got=SUCCEEDED(task->get_Definition(definition.out()))&&SUCCEEDED(definition->get_Principal(principal.out()))&&SUCCEEDED(definition->get_Settings(settings.out()));
    TASK_LOGON_TYPE logon=TASK_LOGON_NONE;TASK_RUNLEVEL_TYPE level=TASK_RUNLEVEL_HIGHEST;
    check(got&&SUCCEEDED(principal->get_LogonType(&logon))&&SUCCEEDED(principal->get_RunLevel(&level))&&logon==TASK_LOGON_INTERACTIVE_TOKEN&&level==TASK_RUNLEVEL_LUA,"interactive desktop and non-elevated principal");
    INT priority=0;check(got&&SUCCEEDED(settings->get_Priority(&priority))&&priority==4,"task priority 4 selects normal interactive scheduling");
    check(contains(xml,L"<MultipleInstancesPolicy>Parallel</MultipleInstancesPolicy>"),"scheduler permits startup in additional sessions");
    check(contains(xml,L"<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>")&&contains(xml,L"<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"),"battery power can start and keep running");
    VARIANT_BOOL network=VARIANT_TRUE,idle=VARIANT_TRUE;
    check(got&&SUCCEEDED(settings->get_RunOnlyIfNetworkAvailable(&network))&&SUCCEEDED(settings->get_RunOnlyIfIdle(&idle))&&!network&&!idle,"no network or idle prerequisite");
    check(contains(xml,L"<StartWhenAvailable>true</StartWhenAvailable>")&&contains(xml,L"<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>"),"start when available and unlimited execution time");
    check(contains(xml,L"<RestartOnFailure>")&&contains(xml,L"<Interval>PT1M</Interval>")&&contains(xml,L"<Count>3</Count>"),"failure restart once a minute up to three times");
    WCHAR exe[32768],directory[32768];GetModuleFileNameW(nullptr,exe,32768);wcscpy(directory,exe);WCHAR *end=wcsrchr(directory,L'\\');if(end)*end=0;
    check(contains(xml,exe)&&contains(xml,directory)&&contains(xml,L"<Arguments>--hidden</Arguments>"),"exact executable, working directory, and hidden arguments");
    SysFreeString(xml);
}
static void probePath(WCHAR *path,size_t count){GetModuleFileNameW(nullptr,path,(DWORD)count);wcscat(path,L".retry.log");}
static int hiddenProbe(){
    WCHAR path[32768+32];GetModuleFileNameW(nullptr,path,32768);wcscat(path,L".priority.log");
    HANDLE priorityFile=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(priorityFile!=INVALID_HANDLE_VALUE){
        char line[32];int bytes=snprintf(line,sizeof(line),"%lu\n",(unsigned long)GetPriorityClass(GetCurrentProcess()));DWORD written=0;
        WriteFile(priorityFile,line,(DWORD)bytes,&written,nullptr);FlushFileBuffers(priorityFile);CloseHandle(priorityFile);
    }
    probePath(path,32768);
    HANDLE file=CreateFileW(path,GENERIC_READ|FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return 0;
    char line[64];int bytes=snprintf(line,sizeof(line),"%llu\n",(unsigned long long)GetTickCount64());DWORD written=0;
    BOOL saved=WriteFile(file,line,(DWORD)bytes,&written,nullptr);FlushFileBuffers(file);CloseHandle(file);
    if(!saved||written!=(DWORD)bytes)return 32;
    WCHAR hold[32768+32];GetModuleFileNameW(nullptr,hold,32768);wcscat(hold,L".hold");
    ULONGLONG start=GetTickCount64();
    while(GetFileAttributesW(hold)!=INVALID_FILE_ATTRIBUTES&&GetTickCount64()-start<10000)Sleep(50);
    return 0;
}
static int probeRecords(LPCWSTR path,unsigned long long *first,unsigned long long *second){
    FILE *file=_wfopen(path,L"rb");if(!file)return 0;
    int records=0;unsigned long long tick;while(fscanf(file,"%llu",&tick)==1){if(records==0)*first=tick;else if(records==1)*second=tick;records++;}fclose(file);return records;
}
static void priorityProbe(ITaskFolder *root){
    WCHAR path[32768+32];GetModuleFileNameW(nullptr,path,32768);wcscat(path,L".priority.log");
    FILE *marker=_wfopen(path,L"wb");check(marker!=nullptr,"isolated process-priority marker created");if(!marker)return;fclose(marker);
    Pointer<IRegisteredTask> task;Pointer<IRunningTask> running;VARIANT empty;VariantInit(&empty);
    bool started=getTask(root,task)&&SUCCEEDED(task->Run(empty,running.out()));check(started,"isolated scheduled task launched for priority probe");
    ULONGLONG start=GetTickCount64();unsigned long observed=0;
    if(started)while(GetTickCount64()-start<5000){
        Sleep(100);FILE *file=_wfopen(path,L"rb");if(file){int fields=fscanf(file,"%lu",&observed);fclose(file);if(fields==1)break;}
    }
    check(observed==NORMAL_PRIORITY_CLASS,"actual scheduler action process has NORMAL_PRIORITY_CLASS");
    printf("PRIORITY_PROBE class=0x%08lX expected=0x%08lX\n",observed,(unsigned long)NORMAL_PRIORITY_CLASS);
    if(started){TASK_STATE state=TASK_STATE_RUNNING;while(GetTickCount64()-start<5000&&SUCCEEDED(task->get_State(&state))&&state==TASK_STATE_RUNNING)Sleep(100);}
    check(DeleteFileW(path)!=FALSE,"process-priority marker cleanup succeeds");
}
static void concurrencyProbe(ITaskFolder *root){
    WCHAR path[32768+32],hold[32768+32];probePath(path,32768);
    GetModuleFileNameW(nullptr,hold,32768);wcscat(hold,L".hold");
    FILE *file=_wfopen(path,L"wb");if(file)fclose(file);check(file!=nullptr,"parallel launch marker created");
    file=_wfopen(hold,L"wb");if(file)fclose(file);check(file!=nullptr,"first action held running for overlap test");
    Pointer<IRegisteredTask> task;Pointer<IRunningTask> firstRun,secondRun;VARIANT empty;VariantInit(&empty);
    bool first=getTask(root,task)&&SUCCEEDED(task->Run(empty,firstRun.out()));check(first,"first overlapping scheduler action requested");
    unsigned long long a=0,b=0;ULONGLONG start=GetTickCount64();
    while(first&&probeRecords(path,&a,&b)<1&&GetTickCount64()-start<4000)Sleep(50);
    TASK_STATE state=TASK_STATE_UNKNOWN;
    check(first&&SUCCEEDED(firstRun->Refresh())&&SUCCEEDED(firstRun->get_State(&state))&&state==TASK_STATE_RUNNING,"first action actually still running before second request");
    check(task.p&&SUCCEEDED(task->Run(empty,secondRun.out())),"second overlapping scheduler action requested");
    while(probeRecords(path,&a,&b)<2&&GetTickCount64()-start<6000)Sleep(50);
    check(probeRecords(path,&a,&b)==2,"both actions execute while first instance is alive");
    check(DeleteFileW(hold)!=FALSE,"overlap hold marker removed");
    if(firstRun.p)while(SUCCEEDED(firstRun->Refresh())&&SUCCEEDED(firstRun->get_State(&state))&&state==TASK_STATE_RUNNING&&GetTickCount64()-start<8000)Sleep(50);
    if(secondRun.p)while(SUCCEEDED(secondRun->Refresh())&&SUCCEEDED(secondRun->get_State(&state))&&state==TASK_STATE_RUNNING&&GetTickCount64()-start<8000)Sleep(50);
    check(DeleteFileW(path)!=FALSE,"parallel launch marker cleanup succeeds");
}
static void repairProbe(ITaskFolder *root){
    check(legacyPresent(L"LiteLaunch-AutoStart"),"independent owned Run fallback registered");
    HKEY key=nullptr;LONG r=RegOpenKeyExW(HKEY_CURRENT_USER,qaKey,0,KEY_SET_VALUE,&key);
    if(r==ERROR_SUCCESS){r=RegDeleteValueW(key,L"LiteLaunch-AutoStart");RegCloseKey(key);}
    check(r==ERROR_SUCCESS,"fallback removed to reproduce partial startup registration");
    apiCheck(repairStartup(),"owned enabled task repairs its missing fallback on application launch");
    check(legacyPresent(L"LiteLaunch-AutoStart"),"missing Run fallback restored");
    String name(qaTask);check(SUCCEEDED(root->DeleteTask(name.p,0)),"task removed to reproduce partial registration");
    apiCheck(repairStartup(),"owned fallback repairs its missing task on application launch");
    Pointer<IRegisteredTask> task;check(getTask(root,task),"missing task restored");
    if(task.p){
        check(SUCCEEDED(task->put_Enabled(VARIANT_FALSE)),"fixture task explicitly disabled");
        apiCheck(repairStartup(),"startup audit respects user-disabled task");
        VARIANT_BOOL enabled=VARIANT_TRUE;check(SUCCEEDED(task->get_Enabled(&enabled))&&!enabled,"audit does not re-enable disabled task");
    }
    apiCheck(setStartup(TRUE),"explicit enable restores both channels after audit");
    {
        Pointer<IRegisteredTask> old;Pointer<ITaskDefinition> definition;Pointer<ITaskSettings> settings;
        bool got=getTask(root,old)&&SUCCEEDED(old->get_Definition(definition.out()))&&SUCCEEDED(definition->get_Settings(settings.out()));
        check(got&&SUCCEEDED(settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW)),"old IgnoreNew policy fixture prepared");
        String userSid(sid);VARIANT user,empty;VariantInit(&user);VariantInit(&empty);user.vt=VT_BSTR;user.bstrVal=userSid.p;
        Pointer<IRegisteredTask> updated;
        check(got&&SUCCEEDED(root->RegisterTaskDefinition(name.p,definition.p,TASK_CREATE_OR_UPDATE,user,empty,TASK_LOGON_INTERACTIVE_TOKEN,empty,updated.out())),"old enabled task policy registered");
        apiCheck(repairStartup(),"ordinary launch upgrades old enabled registration");
        Pointer<IRegisteredTask> repaired;Pointer<ITaskDefinition> current;Pointer<ITaskSettings> options;TASK_INSTANCES_POLICY policy=TASK_INSTANCES_IGNORE_NEW;
        check(getTask(root,repaired)&&SUCCEEDED(repaired->get_Definition(current.out()))&&SUCCEEDED(current->get_Settings(options.out()))&&SUCCEEDED(options->get_MultipleInstances(&policy))&&policy==TASK_INSTANCES_PARALLEL,"old task upgraded to Parallel");
    }
    key=nullptr;r=RegOpenKeyExW(HKEY_CURRENT_USER,qaKey,0,KEY_SET_VALUE,&key);
    const WCHAR collision[]=L"notepad.exe --hidden";
    if(r==ERROR_SUCCESS){r=RegSetValueExW(key,L"LiteLaunch-AutoStart",0,REG_SZ,(const BYTE*)collision,sizeof(collision));RegCloseKey(key);}
    check(r==ERROR_SUCCESS,"unrelated fallback command fixture created");
    check(!setStartup(TRUE)&&startupLastError()==E_ACCESSDENIED,"enable refuses unrelated fallback command");
    check(!setStartup(FALSE)&&startupLastError()==E_ACCESSDENIED,"disable refuses unrelated fallback command");
    check(!repairStartup()&&startupLastError()==E_ACCESSDENIED,"automatic repair refuses unrelated fallback command");
    WCHAR observed[64]={0};DWORD bytes=sizeof(observed);key=nullptr;
    r=RegOpenKeyExW(HKEY_CURRENT_USER,qaKey,0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key);
    if(r==ERROR_SUCCESS){r=RegQueryValueExW(key,L"LiteLaunch-AutoStart",nullptr,nullptr,(BYTE*)observed,&bytes);RegDeleteValueW(key,L"LiteLaunch-AutoStart");RegCloseKey(key);}
    check(r==ERROR_SUCCESS&&!wcscmp(observed,collision),"unrelated fallback preserved exactly");
    apiCheck(setStartup(TRUE),"owned fallback restored after isolated collision cleanup");
}
static void retryProbe(ITaskFolder *root){
    WCHAR exe[32768],target[32768+64],path[32768+96];GetModuleFileNameW(nullptr,exe,32768);
    swprintf(target,32768+64,L"%ls.retry-probe.exe",exe);swprintf(path,32768+96,L"%ls.retry.log",target);
    check(GetFileAttributesW(target)==INVALID_FILE_ATTRIBUTES,"retry action executable initially absent");
    check(rewrite(root,target,nullptr),"isolated task action points to the absent retry executable");
    FILE *marker=_wfopen(path,L"wb");
    check(marker!=nullptr,"isolated failure-retry marker created");if(!marker)return;fclose(marker);
    Pointer<IRegisteredTask> task;Pointer<IRunningTask> running;VARIANT empty;VariantInit(&empty);
    HRESULT requested=getTask(root,task)?task->Run(empty,running.out()):E_FAIL;
    bool started=SUCCEEDED(requested);check(started,"isolated scheduler task requested for action-failure retry probe");
    ULONGLONG start=GetTickCount64();unsigned long long first=0,second=0;int records=0;
    bool supplied=false,failedAction=false;
    if(started)while(GetTickCount64()-start<85000){
        Sleep(1000);records=probeRecords(path,&first,&second);
        if(!supplied&&GetTickCount64()-start>=10000){
            LONG result=0;HRESULT read=task->get_LastTaskResult(&result);
            printf("PROBE_INITIAL_RESULT hresult=0x%08lX\n",(unsigned long)result);
            failedAction=SUCCEEDED(read)&&result==HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)&&records==0;
            supplied=CopyFileW(exe,target,TRUE)!=FALSE;check(supplied,"retry executable supplied after the initial missing-action failure");
        }
        if(records>=1)break;
        if((GetTickCount64()-start)/1000%15==0){printf("PROBE_WAIT seconds=%llu launches=%d\n",(unsigned long long)((GetTickCount64()-start)/1000),records);fflush(stdout);}
    }
    check(failedAction,"initial launch failed because its action executable was missing");
    check(records==1,"scheduler retried the failed action and launched the recovered executable");
    check(records==1&&first-start>=55000&&first-start<=80000,"action retry started approximately 60 seconds after initial failure");
    printf("PROBE_RESULT launches=%d first_tick=%llu initial_request_tick=%llu interval_ms=%llu\n",records,first,(unsigned long long)start,first>=start?first-start:0);
    check(DeleteFileW(path)!=FALSE,"failure-retry marker cleanup succeeds");
    check(DeleteFileW(target)!=FALSE,"retry executable copy cleanup succeeds");
}
int wmain(int argc,WCHAR **argv){
    if(argc>1&&!wcscmp(argv[1],L"--hidden"))return hiddenProbe();
    bool skipRetry=argc>1&&!wcscmp(argv[1],L"--skip-retry");
    DWORD pid=GetCurrentProcessId();swprintf(qaTask,256,L"LiteLaunch-QA-%lu-%llu",pid,(unsigned long long)GetTickCount64());
    swprintf(qaRoot,512,L"Software\\LiteLaunchStartupQA\\%lu-%llu",pid,(unsigned long long)GetTickCount64());swprintf(qaKey,512,L"%ls\\Run",qaRoot);
    printf("QA_TASK=%ls\nQA_REGISTRY=HKCU\\%ls\n",qaTask,qaRoot);
    check(!startupTestConfigure(L"LiteLaunch-AutoStart-production",qaKey),"test namespace rejects production task");
    check(!startupTestConfigure(qaTask,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"),"test namespace rejects production Run key");
    if(!startupTestConfigure(qaTask,qaKey)||!currentSid())return 90;
    HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(init))return 91;
    {
        Pointer<ITaskService> service;Pointer<ITaskFolder> root;
        if(!folder(service,root)){CoUninitialize();return 92;}
        apiCheck(repairStartup(),"clean fixture audit does not opt user into startup");
        check(!startupEnabled()&&SUCCEEDED(startupLastError()),"clean fixture reports startup disabled");
        WCHAR exe[32768],command[32768+32];GetModuleFileNameW(nullptr,exe,32768);swprintf(command,32768+32,L"\"%ls\" --hidden",exe);
        check(setLegacy(command),"isolated legacy startup command created");
        check(startupEnabled(),"owned legacy Run is recognized");
        apiCheck(setStartup(TRUE),"scheduled startup enabled and read back");
        check(!legacyPresent(),"owned legacy Run removed after successful task registration");
        check(startupEnabled(),"enabled scheduled startup recognized");
        verifyXml(root.p);
        priorityProbe(root.p);
        concurrencyProbe(root.p);
        repairProbe(root.p);
        if(skipRetry)puts("SKIP action-failure retry probe by explicit --skip-retry; previously verified separately");else retryProbe(root.p);
        apiCheck(setStartup(TRUE),"repeat enable updates the owned task safely");
        check(rewrite(root.p,L"C:\\Windows\\System32\\notepad.exe",nullptr),"fixture task changed to an unrelated path");
        check(!startupEnabled()&&startupLastError()==E_ACCESSDENIED,"unrelated path is not reported as this copy's startup");
        check(!setStartup(FALSE)&&startupLastError()==E_ACCESSDENIED,"disable refuses an unrelated task path");
        {Pointer<IRegisteredTask> task;check(getTask(root.p,task),"protected unrelated-path task remains registered");}
        apiCheck(setStartup(TRUE),"explicit enable repairs an owned task's path");
        check(setLegacy(L"\"C:\\Windows\\System32\\notepad.exe\" --hidden"),"unrelated legacy command fixture created");
        apiCheck(setStartup(TRUE),"enable leaves unrelated legacy command untouched");
        check(legacyPresent(),"unrelated Run value remains after enable");
        apiCheck(setStartup(FALSE),"scheduled startup disabled without stopping this process");
        check(!startupEnabled(),"disabled task no longer enabled");
        check(!legacyPresent(L"LiteLaunch-AutoStart"),"disable removes the owned fallback as well");
        check(legacyPresent(),"unrelated Run value remains after disable");
        apiCheck(setStartup(FALSE),"repeat disable succeeds");
        apiCheck(setStartup(TRUE),"task re-created for ownership collision test");
        check(rewrite(root.p,nullptr,L"AnotherProduct"),"fixture task author changed to unrelated owner");
        check(!setStartup(TRUE)&&startupLastError()==E_ACCESSDENIED,"enable refuses unrelated owner collision");
        check(!setStartup(FALSE)&&startupLastError()==E_ACCESSDENIED,"disable refuses unrelated owner collision");
        /* Test owns this unique namespace, so direct cleanup can remove its collision fixture. */
        String name(qaTask);check(SUCCEEDED(root->DeleteTask(name.p,0)),"isolated task cleanup succeeds");
        Pointer<IRegisteredTask> gone;check(!getTask(root.p,gone),"QA task absent after cleanup");
    }
    LONG deleted=RegDeleteTreeW(HKEY_CURRENT_USER,qaRoot);check(deleted==ERROR_SUCCESS||deleted==ERROR_FILE_NOT_FOUND,"isolated registry fixture cleanup succeeds");
    CoUninitialize();printf("RESULT checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
