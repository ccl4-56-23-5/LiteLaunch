/* Uses the production config functions but never opens the real user profile. */
#define LITELAUNCH_CONFIG_TEST
#define wWinMain LiteLaunchUiEntry
#include "main.c"
#undef wWinMain
#define CHECK(x) do { if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} }while(0)

static BOOL corrupt(LPCWSTR path) {
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return FALSE;
    DWORD written=0;BOOL ok=WriteFile(file,"bad",3,&written,NULL)&&written==3;
    CloseHandle(file);return ok;
}
static BOOL truncateBlacklist(LPCWSTR path) {
    HANDLE file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return FALSE;
    LARGE_INTEGER size={0};DWORD read=0;BOOL ok=GetFileSizeEx(file,&size)&&size.QuadPart>0&&size.QuadPart<4*1024*1024;
    BYTE *bytes=ok?(BYTE*)malloc((size_t)size.QuadPart+2):NULL;
    if(!bytes)ok=FALSE;
    if(ok)ok=ReadFile(file,bytes,(DWORD)size.QuadPart,&read,NULL)&&read==(DWORD)size.QuadPart;
    if(ok){bytes[read]=0;bytes[read+1]=0;WCHAR *section=wcsstr((WCHAR*)bytes,L"[Blacklist]");
        ok=section!=NULL;
        if(ok){LARGE_INTEGER end={0};end.QuadPart=(BYTE*)section-bytes;
            ok=SetFilePointerEx(file,end,NULL,FILE_BEGIN)&&SetEndOfFile(file);}
    }
    free(bytes);CloseHandle(file);return ok;
}
static void useEntry(LPCWSTR name,LPCWSTR target) {
    ZeroMemory(entries,sizeof(entries));
    lstrcpynW(entries[0].name,name,128);
    lstrcpynW(entries[0].target,target,2048);
}
int wmain(int argc,WCHAR **argv) {
    CHECK(argc==2&&validConfig(argv[1]));
    WCHAR temporary[MAX_PATH],root[MAX_PATH],central[MAX_PATH],old[MAX_PATH];
    DWORD length=GetTempPathW(MAX_PATH,temporary);
    CHECK(length&&length<MAX_PATH);
    swprintf(root,MAX_PATH,L"%lsLiteLaunch-config-test-%lu",temporary,GetCurrentProcessId());
    CHECK(CreateDirectoryW(root,NULL)||GetLastError()==ERROR_ALREADY_EXISTS);
    swprintf(central,MAX_PATH,L"%ls\\central",root);
    swprintf(old,MAX_PATH,L"%ls\\legacy",root);
    CHECK(CreateDirectoryW(central,NULL)||GetLastError()==ERROR_ALREADY_EXISTS);
    CHECK(CreateDirectoryW(old,NULL)||GetLastError()==ERROR_ALREADY_EXISTS);
    CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);glassInit();

    CHECK(setConfigPaths(old,old));
    useEntry(L"旧配置",L"C:\\Fixture\\original.exe");
    for(int i=1;i<11;i++){
        swprintf(entries[i].name,128,L"应用%d",i);
        swprintf(entries[i].target,2048,L"C:\\Fixture\\app%d.exe",i);
    }
    blacklistCount=1;lstrcpyW(blacklist[0],L"Fixture.exe");
    CHECK(saveConfig());CHECK(validConfig(config));CHECK(validConfig(configBackup));
    CHECK(setConfigPaths(central,old));
    CHECK(loadConfig());
    CHECK(!wcscmp(entries[0].name,L"旧配置"));
    CHECK(validConfig(config)&&validConfig(configBackup)&&validConfig(configInitial));
    CHECK(configuredTargets(configInitial)==11);
    puts("PASS one-time migration from adjacent legacy INI to central INI with backup");

    CHECK(corrupt(configInitial));CHECK(!loadConfig());CHECK(configFailure[0]);
    CHECK(!validConfig(configInitial)&&validConfig(config));
    CHECK(CopyFileW(config,configInitial,FALSE));CHECK(loadConfig());
    puts("PASS invalid initial snapshot is reported and preserved until explicit repair");

    CHECK(truncateBlacklist(config));CHECK(!validConfig(config));
    CHECK(loadConfig());CHECK(blacklistCount==1&&validConfig(config));
    puts("PASS truncated blacklist tail is rejected and restored from backup");

    CHECK(setConfigPaths(old,old));
    useEntry(L"过期旧配置",L"C:\\Fixture\\stale.exe");CHECK(saveConfig());
    CHECK(setConfigPaths(central,old));ZeroMemory(entries,sizeof(entries));CHECK(loadConfig());
    CHECK(!wcscmp(entries[0].name,L"旧配置"));
    puts("PASS existing central INI takes precedence over adjacent stale INI");

    useEntry(L"新配置",L"C:\\Fixture\\updated.exe");CHECK(saveConfig());
    WCHAR previous[128];GetPrivateProfileStringW(L"Item0",L"Name",L"",previous,128,configBackup);
    CHECK(!wcscmp(previous,L"旧配置"));
    CHECK(corrupt(config));CHECK(!validConfig(config));CHECK(loadConfig());
    CHECK(!wcscmp(entries[0].name,L"旧配置"));CHECK(validConfig(config));
    puts("PASS save retains previous valid backup and corrupt main restores it");

    useEntry(L"退化一次",L"C:\\Fixture\\one.exe");CHECK(saveConfig());
    useEntry(L"退化两次",L"C:\\Fixture\\two.exe");CHECK(saveConfig());
    CHECK(configuredTargets(config)==1&&configuredTargets(configBackup)==1);
    CHECK(validConfig(configInitial)&&configuredTargets(configInitial)==11);
    puts("PASS two degraded saves cannot overwrite the initial 11-entry recovery snapshot");

    CHECK(corrupt(config));CHECK(corrupt(configBackup));
    CHECK(loadConfig());CHECK(configuredTargets(config)==11);
    puts("PASS corrupt main and rolling backup restore the initial snapshot");

    CHECK(corrupt(config));CHECK(corrupt(configBackup));CHECK(corrupt(configInitial));
    CHECK(!loadConfig());CHECK(configFailure[0]);
    CHECK(!validConfig(config)&&!validConfig(configBackup)&&!validConfig(configInitial));
    puts("PASS all three corrupt copies block startup without overwriting them");

    CHECK(CopyFileW(legacyConfig,configBackup,FALSE));CHECK(DeleteFileW(config));
    CHECK(!loadConfig());CHECK(!validConfig(configInitial));
    CHECK(DeleteFileW(configInitial));
    ZeroMemory(entries,sizeof(entries));CHECK(loadConfig());
    CHECK(!wcscmp(entries[0].name,L"过期旧配置"));CHECK(validConfig(config));
    puts("PASS missing main restores an existing valid backup");

    CHECK(DeleteFileW(config));CHECK(DeleteFileW(configBackup));
    if(fileExists(configInitial))CHECK(DeleteFileW(configInitial));
    CHECK(corrupt(legacyConfig));
    CHECK(!loadConfig());CHECK(configFailure[0]);CHECK(!fileExists(config));
    puts("PASS invalid legacy INI blocks first-run default and preserves source");

    CHECK(DeleteFileW(legacyConfig));ZeroMemory(entries,sizeof(entries));
    CHECK(loadConfig());CHECK(!wcscmp(entries[0].name,L"Codex"));
    CHECK(saveConfig());CHECK(validConfig(config)&&validConfig(configBackup));
    CHECK(!fileExists(configInitial));
    puts("PASS genuinely fresh install creates Codex default and a backup on first save");

    CHECK(DeleteFileW(config));CHECK(DeleteFileW(configBackup));
    CHECK(CopyFileW(argv[1],legacyConfig,FALSE));
    ZeroMemory(entries,sizeof(entries));CHECK(loadConfig());
    CHECK(!wcscmp(entries[1].name,L"百度云"));
    CHECK(!wcscmp(entries[11].name,L"Codex"));
    CHECK(themeIndex==4&&validConfig(config)&&validConfig(configBackup));
    puts("PASS actual 1.0.9 preset format migrates with order, names and theme");

    for(int i=0;i<COUNT;i++)if(entries[i].visual)glassFreeIcon(entries[i].visual);
    glassShutdown();CoUninitialize();return 0;
}
