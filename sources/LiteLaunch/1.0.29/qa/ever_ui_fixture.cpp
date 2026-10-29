// Standalone manual EVER UI fixture. No production profile or startup access.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define LITELAUNCH_EVER_TESTING
#include "../rename.cpp"
#include <cstdio>

static void fixtureWrite(const std::wstring &path, const char *content) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Private fixture file creation failed");
    DWORD written=0;bool ok=WriteFile(file,content,(DWORD)strlen(content),&written,nullptr)&&written==strlen(content);CloseHandle(file);
    if(!ok)throw std::runtime_error("Private fixture write failed");
}
int wmain(int argc,wchar_t **argv) {
    try {
        // Consume STARTUPINFO's first ShowWindow override on this fixture's console.
        // A Hidden launcher must not suppress the later manual editor window.
        HWND console = GetConsoleWindow();
        if (console) ShowWindow(console, SW_HIDE);
        bool empty=false,twice=false;
        for(int i=1;i<argc;++i){if(!wcscmp(argv[i],L"--empty"))empty=true;else if(!wcscmp(argv[i],L"--twice"))twice=true;else throw std::runtime_error("Usage: ever-ui-fixture [--empty] [--twice]");}
        SetProcessDPIAware();
        wchar_t executable[32768]={};if(!GetModuleFileNameW(nullptr,executable,32768))throw std::runtime_error("Fixture executable unavailable");
        GUID id={};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Fixture GUID unavailable");wchar_t token[80]={};StringFromGUID2(id,token,80);
        std::wstring root=llrename::parent(executable)+L"LL_EVER_UI_QA_"+token;
        if(!CreateDirectoryW(root.c_str(),nullptr))throw std::runtime_error("Private fixture directory creation failed");
        std::wstring state=root+L"\\state";if(!CreateDirectoryW(state.c_str(),nullptr))throw std::runtime_error("Private fixture state creation failed");
        llrename::eTestHome=state+L"\\";
        fixtureWrite(root+L"\\qa-owned.txt","LiteLaunch EVER manual UI fixture v1");
        if(!empty){
            const wchar_t *names[]={L"01_示例甲.txt",L"02_示例乙.txt",L"03_示例丙.txt"};
            for(int i=0;i<3;++i){std::wstring path=root+L"\\"+names[i];std::string content="Private sample object "+std::to_string(i+1)+"\r\n";fixtureWrite(path,content.c_str());llrename::eTestPaths.push_back(path);}
        }
        std::printf("PRIVATE_ROOT=%s\n",llrename::eUtf8(root).c_str());
        std::printf("PROFILE=%s\n",llrename::eUtf8(llrename::eTestHome).c_str());
        std::printf("SAMPLES=%zu\n",llrename::eTestPaths.size());
        std::fflush(stdout);
        renameShow(nullptr,nullptr,96);
        if(twice){llrename::eTestPaths.clear();std::printf("REOPEN_SAME_PRIVATE_PROFILE=1\n");std::fflush(stdout);renameShow(nullptr,nullptr,96);}
        Scintilla_ReleaseResources();
        return 0;
    } catch(const std::exception &error) {std::fprintf(stderr,"FIXTURE ERROR: %s Win32=%lu\n",error.what(),GetLastError());return 1;}
}
