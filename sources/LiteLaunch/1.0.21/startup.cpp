#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <taskschd.h>
#include <sddl.h>
#include <shellapi.h>
#include <wchar.h>
#include "startup.h"

namespace {
static HRESULT lastError=S_OK;
static const WCHAR owner[]=L"LiteLaunch";
#ifndef LITELAUNCH_STARTUP_TESTING
static const WCHAR runKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
#else
static WCHAR qaName[256],qaKey[512];
#endif
template<class T> struct ComPtr {
    T *p=nullptr;
    ~ComPtr(){if(p)p->Release();}
    T **out(){return &p;}
    T *operator->()const{return p;}
};
struct Bstr {
    BSTR p;
    explicit Bstr(LPCWSTR text):p(SysAllocString(text)){}
    ~Bstr(){SysFreeString(p);}
};
struct Apartment {
    HRESULT hr;
    Apartment():hr(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)){}
    ~Apartment(){if(SUCCEEDED(hr))CoUninitialize();}
    bool ok()const{return SUCCEEDED(hr)||hr==RPC_E_CHANGED_MODE;}
};
#define CHECK_HR(call) do {HRESULT callResult=(call);if(FAILED(callResult))return callResult;}while(0)
static bool missing(HRESULT hr){return hr==HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)||hr==HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);}
static LPCWSTR legacyKey(){
#ifdef LITELAUNCH_STARTUP_TESTING
    return qaKey;
#else
    return runKey;
#endif
}
static HRESULT identity(WCHAR *sid,size_t sidSize,WCHAR *name,size_t nameSize){
    HANDLE token=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return HRESULT_FROM_WIN32(GetLastError());
    DWORD bytes=0;GetTokenInformation(token,TokenUser,nullptr,0,&bytes);
    if(!bytes){DWORD e=GetLastError();CloseHandle(token);return HRESULT_FROM_WIN32(e);}
    TOKEN_USER *user=(TOKEN_USER*)HeapAlloc(GetProcessHeap(),0,bytes);
    if(!user){CloseHandle(token);return E_OUTOFMEMORY;}
    BOOL got=GetTokenInformation(token,TokenUser,user,bytes,&bytes);
    DWORD error=got?ERROR_SUCCESS:GetLastError();LPWSTR text=nullptr;
    if(got&&!ConvertSidToStringSidW(user->User.Sid,&text))error=GetLastError();
    HeapFree(GetProcessHeap(),0,user);CloseHandle(token);
    if(error)return HRESULT_FROM_WIN32(error);
    int length=swprintf(sid,sidSize,L"%ls",text);LocalFree(text);
    if(length<0||(size_t)length>=sidSize)return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
#ifdef LITELAUNCH_STARTUP_TESTING
    if(!qaName[0]||!qaKey[0])return E_ACCESSDENIED;
    length=swprintf(name,nameSize,L"%ls",qaName);
#else
    length=swprintf(name,nameSize,L"LiteLaunch-AutoStart-%ls",sid);
#endif
    return length<0||(size_t)length>=nameSize?HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER):S_OK;
}
static HRESULT executable(WCHAR *exe,DWORD capacity,WCHAR *directory,DWORD directoryCapacity){
    DWORD n=GetModuleFileNameW(nullptr,exe,capacity);
    if(!n||n>=capacity)return HRESULT_FROM_WIN32(n?ERROR_INSUFFICIENT_BUFFER:GetLastError());
    if(n>=directoryCapacity)return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    wcscpy(directory,exe);WCHAR *end=wcsrchr(directory,L'\\');
    if(!end)return E_FAIL;*end=0;return S_OK;
}
static HRESULT connect(ComPtr<ITaskService>& service,ComPtr<ITaskFolder>& folder){
    CHECK_HR(CoCreateInstance(CLSID_TaskScheduler,nullptr,CLSCTX_INPROC_SERVER,IID_ITaskService,(void**)service.out()));
    VARIANT empty;VariantInit(&empty);
    CHECK_HR(service->Connect(empty,empty,empty,empty));
    Bstr root(L"\\");if(!root.p)return E_OUTOFMEMORY;
    return service->GetFolder(root.p,folder.out());
}
/* Task Scheduler can normalize a supplied SID to DOMAIN\\account on readback. */
static bool sameUser(LPCWSTR value,LPCWSTR sid){
    if(!value)return false;if(!_wcsicmp(value,sid))return true;
    BYTE accountSid[SECURITY_MAX_SID_SIZE];WCHAR domain[256];DWORD sidBytes=sizeof(accountSid),domainChars=256;SID_NAME_USE use;
    if(!LookupAccountNameW(nullptr,value,accountSid,&sidBytes,domain,&domainChars,&use))return false;
    PSID expected=nullptr;if(!ConvertStringSidToSidW(sid,&expected))return false;
    bool equal=EqualSid(accountSid,expected)!=FALSE;LocalFree(expected);return equal;
}
/* An existing task is changed only when our author marker and user SID match. */
static HRESULT owned(IRegisteredTask *task,LPCWSTR sid){
    ComPtr<ITaskDefinition> definition;ComPtr<IRegistrationInfo> info;ComPtr<IPrincipal> principal;
    CHECK_HR(task->get_Definition(definition.out()));
    CHECK_HR(definition->get_RegistrationInfo(info.out()));
    BSTR author=nullptr;CHECK_HR(info->get_Author(&author));
    bool authorMatches=author&&!wcscmp(author,owner);SysFreeString(author);
    if(!authorMatches)return E_ACCESSDENIED;
    CHECK_HR(definition->get_Principal(principal.out()));
    BSTR user=nullptr;CHECK_HR(principal->get_UserId(&user));
    bool same=sameUser(user,sid);SysFreeString(user);
    return same?S_OK:E_ACCESSDENIED;
}
/* An old portable copy cannot disable the task of the currently installed copy. */
static HRESULT currentAction(IRegisteredTask *task){
    ComPtr<ITaskDefinition> definition;ComPtr<IActionCollection> actions;
    ComPtr<IAction> action;ComPtr<IExecAction> exec;
    CHECK_HR(task->get_Definition(definition.out()));
    CHECK_HR(definition->get_Actions(actions.out()));
    LONG count=0;CHECK_HR(actions->get_Count(&count));if(count!=1)return E_ACCESSDENIED;
    CHECK_HR(actions->get_Item(1,action.out()));
    CHECK_HR(action->QueryInterface(IID_IExecAction,(void**)exec.out()));
    BSTR path=nullptr,args=nullptr,workingDirectory=nullptr;
    CHECK_HR(exec->get_Path(&path));HRESULT hr=exec->get_Arguments(&args);
    if(FAILED(hr)){SysFreeString(path);return hr;}
    hr=exec->get_WorkingDirectory(&workingDirectory);
    if(FAILED(hr)){SysFreeString(path);SysFreeString(args);return hr;}
    WCHAR exe[32768],directory[32768];hr=executable(exe,32768,directory,32768);
    bool matches=SUCCEEDED(hr)&&path&&args&&workingDirectory&&!_wcsicmp(path,exe)&&!_wcsicmp(args,L"--hidden")&&!_wcsicmp(workingDirectory,directory);
    SysFreeString(path);SysFreeString(args);SysFreeString(workingDirectory);
    return FAILED(hr)?hr:(matches?S_OK:E_ACCESSDENIED);
}
static HRESULT taskSettings(IRegisteredTask *task,LPCWSTR sid){
    ComPtr<ITaskDefinition> definition;ComPtr<IPrincipal> principal;
    ComPtr<ITriggerCollection> triggers;ComPtr<ITrigger> trigger;ComPtr<ILogonTrigger> logon;
    CHECK_HR(task->get_Definition(definition.out()));
    CHECK_HR(definition->get_Principal(principal.out()));
    TASK_LOGON_TYPE logonType;TASK_RUNLEVEL_TYPE runLevel;
    CHECK_HR(principal->get_LogonType(&logonType));CHECK_HR(principal->get_RunLevel(&runLevel));
    if(logonType!=TASK_LOGON_INTERACTIVE_TOKEN||runLevel!=TASK_RUNLEVEL_LUA)return E_FAIL;
    CHECK_HR(definition->get_Triggers(triggers.out()));LONG count=0;
    CHECK_HR(triggers->get_Count(&count));if(count!=1)return E_FAIL;
    CHECK_HR(triggers->get_Item(1,trigger.out()));
    CHECK_HR(trigger->QueryInterface(IID_ILogonTrigger,(void**)logon.out()));
    BSTR user=nullptr,delay=nullptr;CHECK_HR(logon->get_UserId(&user));
    HRESULT hr=logon->get_Delay(&delay);
    bool matches=SUCCEEDED(hr)&&sameUser(user,sid)&&delay&&!wcscmp(delay,L"PT20S");
    SysFreeString(user);SysFreeString(delay);if(FAILED(hr))return hr;if(!matches)return E_FAIL;
    VARIANT_BOOL value=VARIANT_FALSE;CHECK_HR(logon->get_Enabled(&value));if(!value)return E_FAIL;
    ComPtr<ITaskSettings> settings;CHECK_HR(definition->get_Settings(settings.out()));
#define EXPECT_BOOL(method,expected) do {CHECK_HR(settings->method(&value));if((value!=VARIANT_FALSE)!=(expected))return E_FAIL;}while(0)
    EXPECT_BOOL(get_Enabled,true);EXPECT_BOOL(get_AllowDemandStart,true);EXPECT_BOOL(get_StartWhenAvailable,true);
    EXPECT_BOOL(get_DisallowStartIfOnBatteries,false);EXPECT_BOOL(get_StopIfGoingOnBatteries,false);
    EXPECT_BOOL(get_RunOnlyIfNetworkAvailable,false);EXPECT_BOOL(get_RunOnlyIfIdle,false);EXPECT_BOOL(get_WakeToRun,false);
#undef EXPECT_BOOL
    INT retries=0,priority=0;TASK_INSTANCES_POLICY policy;
    CHECK_HR(settings->get_RestartCount(&retries));CHECK_HR(settings->get_MultipleInstances(&policy));CHECK_HR(settings->get_Priority(&priority));
    if(retries!=3||policy!=TASK_INSTANCES_PARALLEL||priority!=4)return E_FAIL;
    BSTR limit=nullptr,restart=nullptr;CHECK_HR(settings->get_ExecutionTimeLimit(&limit));
    hr=settings->get_RestartInterval(&restart);
    matches=SUCCEEDED(hr)&&limit&&restart&&!wcscmp(limit,L"PT0S")&&!wcscmp(restart,L"PT1M");
    SysFreeString(limit);SysFreeString(restart);return FAILED(hr)?hr:(matches?S_OK:E_FAIL);
}
/* This value name is ours, but an unrelated command must be left untouched. */
static HRESULT legacyValue(bool remove,bool *matches){
    *matches=false;HKEY key=nullptr;
    LONG r=RegOpenKeyExW(HKEY_CURRENT_USER,legacyKey(),0,KEY_QUERY_VALUE|(remove?KEY_SET_VALUE:0),&key);
    if(r==ERROR_FILE_NOT_FOUND||r==ERROR_PATH_NOT_FOUND)return S_OK;
    if(r!=ERROR_SUCCESS)return HRESULT_FROM_WIN32(r);
    WCHAR command[32768]={0};DWORD bytes=sizeof(command),type=0;
    r=RegQueryValueExW(key,L"LiteLaunch",nullptr,&type,(BYTE*)command,&bytes);
    if(r==ERROR_FILE_NOT_FOUND){RegCloseKey(key);return S_OK;}
    if(r!=ERROR_SUCCESS){RegCloseKey(key);return HRESULT_FROM_WIN32(r);}
    if(type!=REG_SZ||bytes<sizeof(WCHAR)||bytes>sizeof(command)||bytes%sizeof(WCHAR)||command[bytes/sizeof(WCHAR)-1]){RegCloseKey(key);return S_OK;}
    WCHAR exe[32768],directory[32768];HRESULT hr=executable(exe,32768,directory,32768);
    if(FAILED(hr)){RegCloseKey(key);return hr;}
    int count=0;LPWSTR *args=CommandLineToArgvW(command,&count);
    if(args){
        *matches=(count==1||(count==2&&!_wcsicmp(args[1],L"--hidden")))&&!_wcsicmp(args[0],exe);
        LocalFree(args);
    }
    if(remove&&*matches)r=RegDeleteValueW(key,L"LiteLaunch");
    RegCloseKey(key);return r==ERROR_SUCCESS||r==ERROR_FILE_NOT_FOUND?S_OK:HRESULT_FROM_WIN32(r);
}
/* Independent login fallback. Never replace another command with our value name. */
static HRESULT fallbackValue(int operation,bool *matches){
    *matches=false;HKEY key=nullptr;
    LONG r=RegOpenKeyExW(HKEY_CURRENT_USER,legacyKey(),0,KEY_QUERY_VALUE|(operation?KEY_SET_VALUE:0),&key);
    if(r==ERROR_FILE_NOT_FOUND||r==ERROR_PATH_NOT_FOUND){
        if(operation!=1)return S_OK;
        r=RegCreateKeyExW(HKEY_CURRENT_USER,legacyKey(),0,nullptr,0,KEY_QUERY_VALUE|KEY_SET_VALUE,nullptr,&key,nullptr);
    }
    if(r!=ERROR_SUCCESS)return HRESULT_FROM_WIN32(r);
    WCHAR command[32768]={0},exe[32768],directory[32768];DWORD bytes=sizeof(command),type=0;
    HRESULT hr=executable(exe,32768,directory,32768);
    if(FAILED(hr)){RegCloseKey(key);return hr;}
    r=RegQueryValueExW(key,L"LiteLaunch-AutoStart",nullptr,&type,(BYTE*)command,&bytes);
    bool present=r==ERROR_SUCCESS;
    if(present&&type==REG_SZ&&bytes>=sizeof(WCHAR)&&bytes<=sizeof(command)&&!(bytes%sizeof(WCHAR))&&!command[bytes/sizeof(WCHAR)-1]){
        int count=0;LPWSTR *args=CommandLineToArgvW(command,&count);
        if(args){*matches=count==2&&!_wcsicmp(args[0],exe)&&!wcscmp(args[1],L"--hidden");LocalFree(args);}
    }
    if(r!=ERROR_SUCCESS&&r!=ERROR_FILE_NOT_FOUND){RegCloseKey(key);return HRESULT_FROM_WIN32(r);}
    if(present&&!*matches){RegCloseKey(key);return E_ACCESSDENIED;}
    if(operation==1){
        int length=swprintf(command,32768,L"\"%ls\" --hidden",exe);
        /* Windows Run commands are limited to 260 characters. */
        if(length<0||length>=260){RegCloseKey(key);return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);}
        r=RegSetValueExW(key,L"LiteLaunch-AutoStart",0,REG_SZ,(BYTE*)command,(DWORD)((length+1)*sizeof(WCHAR)));
        if(r==ERROR_SUCCESS)*matches=true;
    }else if(operation==2&&*matches){r=RegDeleteValueW(key,L"LiteLaunch-AutoStart");*matches=false;}
    RegCloseKey(key);return r==ERROR_SUCCESS||r==ERROR_FILE_NOT_FOUND?S_OK:HRESULT_FROM_WIN32(r);
}
static HRESULT registeredEnabled(ITaskFolder *folder,LPCWSTR name,LPCWSTR sid,bool *enabled){
    *enabled=false;Bstr taskName(name);if(!taskName.p)return E_OUTOFMEMORY;
    ComPtr<IRegisteredTask> task;HRESULT hr=folder->GetTask(taskName.p,task.out());
    if(missing(hr))return S_OK;
    CHECK_HR(hr);CHECK_HR(owned(task.p,sid));CHECK_HR(currentAction(task.p));
    VARIANT_BOOL value=VARIANT_FALSE;CHECK_HR(task->get_Enabled(&value));
    if(value!=VARIANT_FALSE)CHECK_HR(taskSettings(task.p,sid));
    *enabled=value!=VARIANT_FALSE;return S_OK;
}
static HRESULT createTask(ITaskService *service,ITaskFolder *folder,LPCWSTR name,LPCWSTR sid){
    Bstr taskName(name),userSid(sid),author(owner),delay(L"PT20S"),limit(L"PT0S"),restart(L"PT1M"),arguments(L"--hidden"),description(L"LiteLaunch: current-user delayed logon startup, version 1.0.21");
    if(!taskName.p||!userSid.p||!author.p||!delay.p||!limit.p||!restart.p||!arguments.p||!description.p)return E_OUTOFMEMORY;
    ComPtr<IRegisteredTask> oldTask;HRESULT hr=folder->GetTask(taskName.p,oldTask.out());
    if(SUCCEEDED(hr))CHECK_HR(owned(oldTask.p,sid));else if(!missing(hr))return hr;
    WCHAR exe[32768],directory[32768];CHECK_HR(executable(exe,32768,directory,32768));
    Bstr path(exe),workingDirectory(directory);if(!path.p||!workingDirectory.p)return E_OUTOFMEMORY;
    ComPtr<ITaskDefinition> definition;CHECK_HR(service->NewTask(0,definition.out()));
    ComPtr<IRegistrationInfo> info;CHECK_HR(definition->get_RegistrationInfo(info.out()));
    CHECK_HR(info->put_Author(author.p));CHECK_HR(info->put_Description(description.p));
    ComPtr<IPrincipal> principal;CHECK_HR(definition->get_Principal(principal.out()));
    CHECK_HR(principal->put_UserId(userSid.p));
    CHECK_HR(principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN));
    CHECK_HR(principal->put_RunLevel(TASK_RUNLEVEL_LUA));
    ComPtr<ITaskSettings> settings;CHECK_HR(definition->get_Settings(settings.out()));
    CHECK_HR(settings->put_Enabled(VARIANT_TRUE));
    CHECK_HR(settings->put_AllowDemandStart(VARIANT_TRUE));
    CHECK_HR(settings->put_StartWhenAvailable(VARIANT_TRUE));
    CHECK_HR(settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE));
    CHECK_HR(settings->put_StopIfGoingOnBatteries(VARIANT_FALSE));
    CHECK_HR(settings->put_RunOnlyIfNetworkAvailable(VARIANT_FALSE));
    CHECK_HR(settings->put_RunOnlyIfIdle(VARIANT_FALSE));
    CHECK_HR(settings->put_ExecutionTimeLimit(limit.p));
    CHECK_HR(settings->put_RestartInterval(restart.p));
    CHECK_HR(settings->put_RestartCount(3));
    CHECK_HR(settings->put_MultipleInstances(TASK_INSTANCES_PARALLEL));
    /* Interactive mouse hooks use normal CPU and memory priority, not background defaults. */
    CHECK_HR(settings->put_Priority(4));
    CHECK_HR(settings->put_WakeToRun(VARIANT_FALSE));
    ComPtr<ITriggerCollection> triggers;CHECK_HR(definition->get_Triggers(triggers.out()));
    ComPtr<ITrigger> trigger;CHECK_HR(triggers->Create(TASK_TRIGGER_LOGON,trigger.out()));
    ComPtr<ILogonTrigger> logon;CHECK_HR(trigger->QueryInterface(IID_ILogonTrigger,(void**)logon.out()));
    CHECK_HR(logon->put_UserId(userSid.p));CHECK_HR(logon->put_Delay(delay.p));
    CHECK_HR(logon->put_Enabled(VARIANT_TRUE));
    ComPtr<IActionCollection> actions;CHECK_HR(definition->get_Actions(actions.out()));
    ComPtr<IAction> action;CHECK_HR(actions->Create(TASK_ACTION_EXEC,action.out()));
    ComPtr<IExecAction> exec;CHECK_HR(action->QueryInterface(IID_IExecAction,(void**)exec.out()));
    CHECK_HR(exec->put_Path(path.p));CHECK_HR(exec->put_Arguments(arguments.p));CHECK_HR(exec->put_WorkingDirectory(workingDirectory.p));
    VARIANT user,empty;VariantInit(&user);VariantInit(&empty);user.vt=VT_BSTR;user.bstrVal=userSid.p;
    ComPtr<IRegisteredTask> registered;
    CHECK_HR(folder->RegisterTaskDefinition(taskName.p,definition.p,TASK_CREATE_OR_UPDATE,user,empty,TASK_LOGON_INTERACTIVE_TOKEN,empty,registered.out()));
    bool enabled=false;CHECK_HR(registeredEnabled(folder,name,sid,&enabled));
    CHECK_HR(taskSettings(registered.p,sid));
    return enabled?S_OK:E_FAIL;
}
static HRESULT apply(bool enable){
    Apartment apartment;if(!apartment.ok())return apartment.hr;
    WCHAR sid[192],name[256];CHECK_HR(identity(sid,192,name,256));
    ComPtr<ITaskService> service;ComPtr<ITaskFolder> folder;CHECK_HR(connect(service,folder));
    bool fallback=false;CHECK_HR(fallbackValue(0,&fallback));
    if(enable){CHECK_HR(createTask(service.p,folder.p,name,sid));CHECK_HR(fallbackValue(1,&fallback));}
    else {
        Bstr taskName(name);if(!taskName.p)return E_OUTOFMEMORY;
        ComPtr<IRegisteredTask> task;HRESULT hr=folder->GetTask(taskName.p,task.out());
        if(SUCCEEDED(hr)){
            CHECK_HR(owned(task.p,sid));CHECK_HR(currentAction(task.p));
            /* Do not Stop(): this can be the running task's own GUI process. */
            CHECK_HR(task->put_Enabled(VARIANT_FALSE));
            CHECK_HR(folder->DeleteTask(taskName.p,0));
        }else if(!missing(hr))return hr;
    }
    if(!enable)CHECK_HR(fallbackValue(2,&fallback));
    bool matches=false;return legacyValue(true,&matches);
}
}
extern "C" HRESULT startupLastError(void){return lastError;}
extern "C" BOOL startupEnabled(void){
    Apartment apartment;
    if(!apartment.ok()){lastError=apartment.hr;return FALSE;}
    WCHAR sid[192],name[256];HRESULT hr=identity(sid,192,name,256);
    bool enabled=false;
    if(SUCCEEDED(hr)){
        ComPtr<ITaskService> service;ComPtr<ITaskFolder> folder;hr=connect(service,folder);
        if(SUCCEEDED(hr))hr=registeredEnabled(folder.p,name,sid,&enabled);
    }
    bool legacy=false;HRESULT legacyHr=legacyValue(false,&legacy);
    lastError=FAILED(hr)?hr:legacyHr;
    bool fallback=false;HRESULT fallbackHr=fallbackValue(0,&fallback);
    if(SUCCEEDED(lastError)&&FAILED(fallbackHr))lastError=fallbackHr;
    return enabled||legacy||(SUCCEEDED(hr)&&fallback);
}
extern "C" BOOL repairStartup(void){
    Apartment apartment;if(!apartment.ok()){lastError=apartment.hr;return FALSE;}
    WCHAR sid[192],name[256];HRESULT hr=identity(sid,192,name,256);
    ComPtr<ITaskService> service;ComPtr<ITaskFolder> folder;
    if(SUCCEEDED(hr))hr=connect(service,folder);
    if(FAILED(hr)){lastError=hr;return FALSE;}
    Bstr taskName(name);if(!taskName.p){lastError=E_OUTOFMEMORY;return FALSE;}
    ComPtr<IRegisteredTask> task;hr=folder->GetTask(taskName.p,task.out());
    bool intent=false;
    if(SUCCEEDED(hr)){
        hr=owned(task.p,sid);if(SUCCEEDED(hr))hr=currentAction(task.p);
        VARIANT_BOOL enabled=VARIANT_FALSE;if(SUCCEEDED(hr))hr=task->get_Enabled(&enabled);
        if(FAILED(hr)){lastError=hr;return FALSE;}
        /* Respect a task explicitly disabled by the user. */
        if(!enabled){lastError=S_OK;return TRUE;}
        intent=true;
        bool fallback=false;HRESULT f=fallbackValue(0,&fallback);
        if(SUCCEEDED(taskSettings(task.p,sid))&&SUCCEEDED(f)&&fallback){lastError=S_OK;return TRUE;}
    }else if(missing(hr)){
        hr=fallbackValue(0,&intent);
        if(SUCCEEDED(hr)&&!intent)hr=legacyValue(false,&intent);
        if(FAILED(hr)){lastError=hr;return FALSE;}
    }else{lastError=hr;return FALSE;}
    lastError=intent?apply(true):S_OK;return SUCCEEDED(lastError);
}
extern "C" BOOL setStartup(BOOL enable){lastError=apply(enable!=FALSE);return SUCCEEDED(lastError);}
#ifdef LITELAUNCH_STARTUP_TESTING
extern "C" BOOL startupTestConfigure(LPCWSTR name,LPCWSTR key){
    if(!name||!key||wcsncmp(name,L"LiteLaunch-QA-",wcslen(L"LiteLaunch-QA-"))||wcsncmp(key,L"Software\\LiteLaunchStartupQA\\",wcslen(L"Software\\LiteLaunchStartupQA\\"))||wcslen(name)>=256||wcslen(key)>=512)return FALSE;
    wcscpy(qaName,name);wcscpy(qaKey,key);return TRUE;
}
#endif
