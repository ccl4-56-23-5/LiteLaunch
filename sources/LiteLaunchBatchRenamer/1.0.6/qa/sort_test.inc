static void sortingTests(const std::wstring&root){
    int previousWarnings=warnings;
    std::wstring home=root+L"\\sorting";CreateDirectoryW(home.c_str(),nullptr);
    const wchar_t*names[]={L"月光12.mp4",L"月光1.avi",L"月光2.mkv"};
    for(int i=0;i<3;i++){
        auto path=home+L"\\"+names[i];{std::ofstream file(path.c_str(),std::ios::binary);file<<std::string(i==0?12:i,char('A'+i));}
        HANDLE file=CreateFileW(path.c_str(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);SYSTEMTIME st={};st.wYear=2026;st.wMonth=1;st.wDay=i==0?3:i;FILETIME created,modified;SystemTimeToFileTime(&st,&created);st.wDay=3-i;SystemTimeToFileTime(&st,&modified);SetFileTime(file,&created,&modified,&modified);CloseHandle(file);
    }
    eSaveSetting(L"Sort",L"FileNameNature");eSaveSetting(L"BatchSimple",L"0");EverWindow editor;for(auto name:names)addPath(editor.items,home+L"\\"+name);editor.sortInitial();
    WNDCLASSEXW cls={sizeof(cls)};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=EverWindow::proc;cls.lpszClassName=L"LiteLaunch.Sort.PrivateQA";RegisterClassExW(&cls);
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"Sort QA",WS_OVERLAPPEDWINDOW,-20000,-20000,1180,740,nullptr,nullptr,cls.hInstance,&editor);
    check(window&&editor.items[0].name==names[1]&&editor.items[1].name==names[2]&&editor.items[2].name==names[0],"natural initial sorting orders 1 2 12 before generating sequence numbers");
    check(SendMessageW(editor.ctl(EverWindow::BR_SORT),CB_GETCOUNT,0,0)==9&&IsWindow(editor.ctl(EverWindow::BR_SORTDIR)),"preview exposes filename photo file-time size extension and original-order controls");
    batch::Step number;number.kind=batch::Kind::Advanced;number.options={{L"type",L"Serialize"},{L"where",L"replace"},{L"prefix",L"S01E"},{L"width",L"2"},{L"start",L"1"}};editor.ruleStack={number};editor.rebuild();
    check(editor.text()==L"S01E01.avi\nS01E02.mkv\nS01E03.mp4","numbered targets follow the natural source order and retain each extension");
    check(editor.batchApplySort(L"FileNameNatureDesc")&&editor.items[0].name==names[0]&&editor.text()==L"S01E01.mp4\nS01E02.mkv\nS01E03.avi","descending sort recomputes numbering for actual source files");
    wchar_t row[100]={};ListView_GetItemText(editor.ctl(EverWindow::BR_FILES),0,0,row,100);check(std::wstring(row)==names[0],"visible preview source rows use the same sorted file identities");
    editor.sci(SCI_UNDO);check(editor.items[0].name==names[1]&&editor.text()==L"S01E01.avi\nS01E02.mkv\nS01E03.mp4","undo sorting restores both file identity and numbered target names");
    editor.sci(SCI_REDO);check(editor.items[0].name==names[0]&&editor.text()==L"S01E01.mp4\nS01E02.mkv\nS01E03.avi","redo sorting restores file identity and extension correspondence");
    editor.setText(L"手工12.mp4\n手工2.mkv\n手工1.avi");
    check(editor.batchApplySort(L"FileNameNature")&&editor.text()==L"手工1.avi\n手工2.mkv\n手工12.mp4","sorting a manual draft moves each edited name with its source file");
    editor.sci(SCI_UNDO);check(editor.items[0].name==names[0]&&editor.text()==L"手工12.mp4\n手工2.mkv\n手工1.avi","manual sort undo preserves the exact filename-to-file mapping");editor.sci(SCI_REDO);
    editor.batchSimple=true;editor.toggle(true);check(editor.batchApplySort(L"FileNameNatureDesc")&&editor.text()==L"手工12\n手工2\n手工1"&&editor.extensions[0]==L".mp4"&&editor.extensions[2]==L".avi","simple-mode sorting retains manual names and hidden extension identities");
    editor.toggle(false);editor.batchSimple=false;editor.rebuild();
    auto orderIs=[&](const wchar_t*mode,int a,int b,int c){return editor.batchApplySort(mode)&&editor.items[0].name==names[a]&&editor.items[1].name==names[b]&&editor.items[2].name==names[c];};
    check(orderIs(L"FileName",1,0,2)&&orderIs(L"FileNameDesc",2,0,1),"alphabetic sorting is available in both directions");
    check(orderIs(L"LastWriteTimeAsc",2,1,0)&&orderIs(L"LastWriteTimeDesc",0,1,2),"modification time sorts actual file timestamps in both directions");
    check(orderIs(L"CreationTimeAsc",1,2,0)&&orderIs(L"CreationTimeDesc",0,2,1),"creation time sorts actual file timestamps in both directions");
    check(orderIs(L"LastAccessTimeAsc",2,1,0)&&orderIs(L"LastAccessTimeDesc",0,1,2),"access time uses its own timestamp values");
    check(orderIs(L"FileSizeAsc",1,2,0)&&orderIs(L"FileSizeDesc",0,2,1),"file size sorts cached values in both directions");
    check(orderIs(L"FileExtensionAsc",1,2,0)&&orderIs(L"FileExtensionDesc",0,2,1),"extension sorting keeps each target extension paired with its source");
    check(orderIs(L"Origin",0,1,2)&&orderIs(L"OriginDesc",2,1,0),"original selection order can be restored and reversed after other sorts");
    int selected=(int)SendMessageW(editor.ctl(EverWindow::BR_SORT),CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)L"修改时间");SendMessageW(editor.ctl(EverWindow::BR_SORT),CB_SETCURSEL,selected,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(EverWindow::BR_SORT,CBN_SELCHANGE),(LPARAM)editor.ctl(EverWindow::BR_SORT));
    check(eSetting(L"Sort",L"")==L"LastWriteTimeDesc"&&editor.items[0].name==names[0],"sort selector notification immediately applies the selected criterion");
    editor.command(EverWindow::BR_SORTDIR);check(eSetting(L"Sort",L"")==L"LastWriteTimeAsc"&&editor.items[0].name==names[2],"direction button changes both the setting and actual item order");
    EverWindow reopened;for(auto name:names)addPath(reopened.items,home+L"\\"+name);reopened.sortInitial();check(reopened.items[0].name==names[2],"a new session applies the saved criterion and direction at import");
    check(orderIs(L"FileNameNatureDesc",0,2,1),"natural descending sequence order is restored for commit");capturePreview(editor,root+L"\\sort-preview.png");
    check(attrs(home+L"\\S01E01.mp4")==INVALID_FILE_ATTRIBUTES,"sorting and numbering previews never rename files by themselves");
    check(editor.execute(),"sorted sequence preview commits using the same source mapping");std::ifstream numbered((home+L"\\S01E01.mp4").c_str(),std::ios::binary);char contents=0;numbered.get(contents);numbered.close();check(contents=='A',"first sequence target contains the originally sorted first file");editor.regret();check(attrs(home+L"\\"+names[0])!=INVALID_FILE_ATTRIBUTES&&attrs(home+L"\\S01E01.mp4")==INVALID_FILE_ATTRIBUTES,"rename undo restores the original files after sorting and numbering");
    auto photo=[&](const wchar_t*name,const char*date){const unsigned char tiff[]={0x49,0x49,0x2a,0,8,0,0,0,1,0,0x69,0x87,4,0,1,0,0,0,26,0,0,0,0,0,0,0,1,0,3,0x90,2,0,20,0,0,0,44,0,0,0,0,0,0,0};std::string payload("Exif\0\0",6);payload.append((const char*)tiff,sizeof(tiff));payload.append(date,20);unsigned length=(unsigned)payload.size()+2;std::ofstream file((home+L"\\"+name).c_str(),std::ios::binary);unsigned char header[]={0xff,0xd8,0xff,0xe1,(unsigned char)(length>>8),(unsigned char)(length&255)};file.write((const char*)header,6);file.write(payload.data(),payload.size());file.put((char)0xff);file.put((char)0xd9);};
    photo(L"late.jpg","2024:01:03 03:04:05\0");photo(L"early2.jpg","2024:01:01 03:04:05\0");photo(L"early1.jpg","2024:01:01 03:04:05\0");{std::ofstream noDate((home+L"\\no-date.txt").c_str());noDate<<"no camera metadata";}
    editor.items.clear();editor.rebuild();for(auto name:{L"no-date.txt",L"late.jpg",L"early2.jpg",L"early1.jpg"})addPath(editor.items,home+L"\\"+name);editor.rebuild();
    check(editor.batchApplySort(L"PhotoTimeAsc")&&editor.items[0].name==L"early1.jpg"&&editor.items[1].name==L"early2.jpg"&&editor.items[2].name==L"late.jpg"&&editor.items[3].name==L"no-date.txt","photo sorting reads EXIF capture time and orders equal times by natural filename");
    check(editor.batchApplySort(L"PhotoTimeDesc")&&editor.items[0].name==L"late.jpg"&&editor.items[1].name==L"early1.jpg"&&editor.items[3].name==L"no-date.txt","descending photo time keeps missing metadata last without using modification time");
    check(warnings==previousWarnings,"sorting with missing capture times does not add confirmation or warning dialogs");DestroyWindow(window);eSaveSetting(L"Sort",L"FileNameNature");
}
