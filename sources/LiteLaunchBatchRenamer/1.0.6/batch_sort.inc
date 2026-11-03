// Members of EverWindow: sort file identities together with their editable names.
std::map<std::wstring,size_t> batchOrigin;
size_t batchNextOrigin=0;
static std::vector<std::wstring> batchSortBases(){return {L"FileNameNature",L"FileName",L"PhotoTime",L"LastWriteTime",L"CreationTime",L"LastAccessTime",L"FileSize",L"FileExtension",L"Origin"};}
static std::vector<std::wstring> batchSortLabels(){return {L"文件名（自然）",L"文件名（字母）",L"拍摄时间",L"修改时间",L"创建时间",L"访问时间",L"文件大小",L"扩展名",L"原始顺序"};}
static bool batchSortDescending(const std::wstring&mode){return mode.size()>4&&mode.substr(mode.size()-4)==L"Desc";}
static std::wstring batchSortBase(std::wstring mode){if(batchSortDescending(mode))mode.resize(mode.size()-4);else if(mode.size()>3&&mode.substr(mode.size()-3)==L"Asc")mode.resize(mode.size()-3);return mode==L"Default"?L"FileNameNature":mode;}
static std::wstring batchSortMode(size_t index,bool descending){auto bases=batchSortBases();if(index>=bases.size())index=0;auto base=bases[index];return base+(descending?L"Desc":index==0||index==1||index==8?L"":L"Asc");}
void batchRememberOriginal(){
    if(items.empty()){batchOrigin.clear();batchNextOrigin=0;return;}
    std::map<std::wstring,size_t> retained;for(const auto&entry:items){auto old=batchOrigin.find(entry.path);retained[entry.path]=old==batchOrigin.end()?batchNextOrigin++:old->second;}batchOrigin.swap(retained);
}
void batchSyncSortControls(){
    if(!ctl(BR_SORT))return;auto mode=eSetting(L"Sort",eDefaultSort()),base=batchSortBase(mode);auto bases=batchSortBases();size_t index=0;for(size_t i=0;i<bases.size();i++)if(base==bases[i])index=i;
    SendMessageW(ctl(BR_SORT),CB_SETCURSEL,index,0);SetWindowTextW(ctl(BR_SORTDIR),batchSortDescending(mode)?L"倒序 ↓":L"正序 ↑");
}
bool batchSortOrder(const std::wstring&mode,std::vector<size_t>&order,std::wstring&error,int&missing){
    struct Key {bool known=true;ULONGLONG number=0;std::wstring text;};
    auto base=batchSortBase(mode);bool descending=batchSortDescending(mode);std::vector<Key>keys(items.size());missing=0;error.clear();
    if(base==L"PhotoTime"&&!items.empty()){
        std::vector<std::vector<std::wstring>>rows;for(const auto&entry:items)rows.push_back({entry.path});std::wstring answer;
        if(!ever::advancedTransform(batch::jsonOptions({{L"type",L"SortDates"}}),batch::jsonRows(rows),answer,error))return false;
        try{auto dates=batch::JsonRows(answer).parse();if(dates.size()!=items.size())throw std::runtime_error("Date key count mismatch");for(size_t i=0;i<items.size();i++){if(dates[i].size()!=1)throw std::runtime_error("Invalid date key");keys[i].text=dates[i][0];keys[i].known=!dates[i][0].empty();}}
        catch(const std::exception&){error=L"拍摄时间读取结果不完整。";return false;}
    }else for(size_t i=0;i<items.size();i++){
        const auto&entry=items[i];auto&key=keys[i];if(base==L"Origin"){auto it=batchOrigin.find(entry.path);key.number=it==batchOrigin.end()?i:it->second;}
        else if(base==L"FileExtension"){key.text=extension(entry);}
        else if(base!=L"FileName"&&base!=L"FileNameNature"){
            WIN32_FILE_ATTRIBUTE_DATA data={};key.known=GetFileAttributesExW(nativePath(entry.path).c_str(),GetFileExInfoStandard,&data)!=FALSE;
            if(key.known){if(base==L"FileSize")key.number=((ULONGLONG)data.nFileSizeHigh<<32)|data.nFileSizeLow;
                else {FILETIME stamp=base==L"CreationTime"?data.ftCreationTime:base==L"LastAccessTime"?data.ftLastAccessTime:data.ftLastWriteTime;key.number=((ULONGLONG)stamp.dwHighDateTime<<32)|stamp.dwLowDateTime;}}
        }
    }
    order.clear();for(size_t i=0;i<items.size();i++){order.push_back(i);if(!keys[i].known)missing++;}
    std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){
        const auto&x=keys[a];const auto&y=keys[b];if(x.known!=y.known)return x.known;
        int compared=0;
        if(x.known){if(base==L"FileNameNature")compared=StrCmpLogicalW(items[a].name.c_str(),items[b].name.c_str());
            else if(base==L"FileName")compared=CompareStringOrdinal(items[a].name.c_str(),-1,items[b].name.c_str(),-1,TRUE)-CSTR_EQUAL;
            else if(base==L"PhotoTime"||base==L"FileExtension")compared=CompareStringOrdinal(x.text.c_str(),-1,y.text.c_str(),-1,TRUE)-CSTR_EQUAL;
            else compared=x.number<y.number?-1:x.number>y.number?1:0;}
        if(compared)return descending?compared>0:compared<0;
        int name=StrCmpLogicalW(items[a].name.c_str(),items[b].name.c_str());if(name)return name<0;return CompareStringOrdinal(items[a].path.c_str(),-1,items[b].path.c_str(),-1,TRUE)==CSTR_LESS_THAN;
    });return true;
}
bool batchApplySort(const std::wstring&mode,bool persist=true){
    batchRememberOriginal();std::vector<std::wstring>current;if(!names(current)){batchSyncSortControls();return false;}
    status(batchSortBase(mode)==L"PhotoTime"?L"正在读取拍摄时间并排序…":L"正在排序…");UpdateWindow(window);
    std::vector<size_t>order;std::wstring error;int missing=0;
    if(!batchSortOrder(mode,order,error,missing)){warn(error);batchSyncSortControls();return false;}
    std::vector<Entry>sorted;std::vector<std::wstring>display,exts;for(auto i:order){sorted.push_back(items[i]);display.push_back(current[i]);exts.push_back(extensions[i]);}
    bool automatic=!batchSimple&&(lastRulePreview.empty()||text()==lastRulePreview);
    if(automatic&&!items.empty()){
        std::vector<batch::Item>inputs;for(const auto&entry:sorted)inputs.push_back({entry.path,entry.name,entry.folder});std::vector<std::wstring>computed;
        if(!batch::preview(ruleStack,inputs,computed,error,&exts)){warn(error);batchSyncSortControls();return false;}display=computed;for(size_t i=0;i<display.size();i++)if(hidden)display[i].resize(display[i].size()-exts[i].size());
    }
    EExtensionUndo action;action.before=action.after=hidden;action.reordered=true;action.oldItems=items;action.newItems=sorted;action.oldExtensions=extensions;action.newExtensions=exts;action.oldPreview=lastRulePreview;action.newPreview=automatic?eJoin(display):L"";
    if(!automatic&&!lastRulePreview.empty()){auto previous=eNameLines(lastRulePreview);if(previous.size()==order.size()){std::vector<std::wstring>moved;for(auto i:order)moved.push_back(previous[i]);action.newPreview=eJoin(moved);}else action.newPreview=lastRulePreview;}
    extensionUndo.push_back(action);sci(SCI_BEGINUNDOACTION);sci(SCI_ADDUNDOACTION,extensionUndo.size(),0);items=std::move(sorted);extensions=std::move(exts);lastRulePreview=action.newPreview;setText(eJoin(display));sci(SCI_ENDUNDOACTION);
    if(persist)eSaveSetting(L"Sort",mode);batchSyncSortControls();batchFiles();
    auto labels=batchSortLabels(),bases=batchSortBases();size_t index=0;for(size_t i=0;i<bases.size();i++)if(batchSortBase(mode)==bases[i])index=i;
    std::wstring message=L"已按"+labels[index]+(batchSortDescending(mode)?L"倒序":L"正序")+L"排列；"+(automatic?L"规则预览已按新顺序重算。":L"预览名称随文件一起移动。");
    if(missing)message+=L" "+std::to_wstring(missing)+L" 项无可用时间，放在末尾。";status(message);return true;
}
void batchSortChanged(bool flip){
    int index=(int)SendMessageW(ctl(BR_SORT),CB_GETCURSEL,0,0);if(index<0)return;bool descending=batchSortDescending(eSetting(L"Sort",eDefaultSort()));if(flip)descending=!descending;batchApplySort(batchSortMode((size_t)index,descending));
}
