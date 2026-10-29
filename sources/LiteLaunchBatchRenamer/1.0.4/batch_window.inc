// Members of EverWindow, compiled only into the standalone Batch Renamer plugin.
enum { BR_RULES=2000,BR_FILES,BR_HEADING,BR_MANUAL,BR_ADD,BR_EDIT,BR_UP,BR_DOWN,BR_DELETE,BR_CLEAR,BR_PREVIEW,BR_SAVE,BR_LOAD,BR_LEGACY,
 BR_SIMPLE,BR_ADVANCED,BR_QADD,BR_QDELETE,BR_QREPLACE,BR_QNUMBER,BR_QUPPER,BR_QLOWER,BR_QSPACE,BR_QRESTORE,BR_QUNDO,BR_QREDO };
std::vector<batch::Step> ruleStack;
bool batchSimple=true;
bool batchUpdating=false;
bool batchLegacyAction=false;
std::wstring lastRulePreview;
HFONT batchTitleFont=nullptr;
HIMAGELIST batchRowImages=nullptr;
RECT batchRuleCard={},batchTableCard={},batchEditCard={};
void batchMore(){
    HMENU menu=CreatePopupMenu();
    const int ids[]={EE_UNDO,EE_REDO,EE_FIND,EE_RULE,EE_NAMING,EE_EXT,EE_LINK,EE_COPY,EE_SPEAK,EE_ZOOMIN,EE_ZOOMOUT,EE_REGRET,EE_OTHER};
    const wchar_t*labels[]={L"撤销编辑\tCtrl+Z",L"重做编辑\tCtrl+Y",L"查找替换…",L"增删转换…",L"编号与元数据…",L"扩展名显示…",L"联动编辑器…",L"复制名称",L"朗读名称",L"放大编辑文字",L"缩小编辑文字",L"撤回上次重命名",L"其他操作与设置…"};
    for(int i=0;i<13;i++){if(i==2||i==7||i==11)AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,ids[i],labels[i]);}
    RECT box;GetWindowRect(ctl(EE_OTHER),&box);int id=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,box.left,box.bottom+4,0,window,nullptr);DestroyMenu(menu);
    if(id==EE_OTHER)this->menu(EE_OTHER);else if(id)command(id);
}
void batchText(HDC dc,const std::wstring&value,RECT box,COLORREF color,HFONT useFont=nullptr,UINT flags=DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS){
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);SelectObject(dc,useFont?useFont:font);DrawTextW(dc,value.c_str(),-1,&box,flags|DT_NOPREFIX);
}
void batchPaint(HDC dc,const RECT&client){
    HBRUSH brush=CreateSolidBrush(UI_CANVAS);FillRect(dc,&client,brush);DeleteObject(brush);
    RECT head={0,0,client.right,px(90)};FillRect(dc,&head,(HBRUSH)GetStockObject(WHITE_BRUSH));
    RECT badge={px(24),px(23),px(68),px(67)};eRoundRect(dc,badge,px(13),uiAccentWash(glassAccentColor(),12),uiAccentWash(glassAccentColor(),12));
    HICON icon=(HICON)LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,px(38),px(38),LR_SHARED);
    if(icon)DrawIconEx(dc,px(27),px(26),icon,px(38),px(38),0,nullptr,DI_NORMAL);
    else batchText(dc,L"Aa",badge,glassAccentColor(),titleFont,DT_SINGLELINE|DT_VCENTER|DT_CENTER);
    batchText(dc,L"批量重命名",{px(82),px(18),px(350),px(49)},UI_INK,batchTitleFont);
    batchText(dc,L"按顺序组合规则，让每个文件名恰到好处",{px(83),px(49),client.right-px(520),px(72)},UI_MUTED);
    auto card=[&](RECT r){RECT shadow=r;OffsetRect(&shadow,0,px(2));eRoundRect(dc,shadow,px(16),RGB(234,238,245),RGB(234,238,245));eRoundRect(dc,r,px(16),UI_SURFACE,UI_LINE);};
    card(batchRuleCard);card(batchTableCard);card(batchEditCard);
    int x=batchRuleCard.left+px(18),y=batchRuleCard.top;
    batchText(dc,batchSimple?L"简易处理 · EVER":L"规则队列",{x,y+px(12),batchRuleCard.right-px(12),y+px(39)},UI_INK,titleFont);
    batchText(dc,batchSimple?L"点选工具，直接处理当前预览":L"从上到下执行 · 勾选启用",{x,y+px(82),batchRuleCard.right-px(12),y+px(105)},UI_MUTED);
    x=batchTableCard.left+px(18);y=batchTableCard.top;
    batchText(dc,L"文件预览",{x,y+px(12),x+px(150),y+px(39)},UI_INK,titleFont);
    batchText(dc,std::to_wstring(items.size())+L" 个项目",{x+px(100),y+px(14),x+px(250),y+px(38)},UI_MUTED);
    int available=batchTableCard.right-batchTableCard.left-px(36),col=(available-px(116))/2;
    RECT strip={x,y+px(53),batchTableCard.right-px(18),y+px(85)};eRoundRect(dc,strip,px(7),UI_SURFACE_ALT,UI_SURFACE_ALT);
    batchText(dc,L"原名称",{x+px(12),strip.top,x+col,strip.bottom},UI_MUTED);
    batchText(dc,L"新名称",{x+col+px(12),strip.top,x+2*col,strip.bottom},UI_MUTED);
    batchText(dc,L"状态",{x+2*col+px(10),strip.top,strip.right,strip.bottom},UI_MUTED);
    if(items.empty())batchText(dc,L"拖入文件或文件夹，开始整理名称",{x,y+px(108),strip.right,batchTableCard.bottom-px(16)},UI_FAINT,nullptr,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
    x=batchEditCard.left+px(18);y=batchEditCard.top;
    batchText(dc,L"手工微调",{x,y+px(12),x+px(100),y+px(39)},UI_INK,titleFont);
    batchText(dc,L"每行一个名称 · 可直接编辑",{x+px(100),y+px(14),batchEditCard.right-px(136),y+px(38)},UI_MUTED);
}
LRESULT batchCustomDraw(NMLVCUSTOMDRAW*draw){
    if(draw->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW|CDRF_NOTIFYPOSTPAINT;
    if(draw->nmcd.dwDrawStage==CDDS_POSTPAINT&&ListView_GetItemCount(draw->nmcd.hdr.hwndFrom)==0){RECT box;GetClientRect(draw->nmcd.hdr.hwndFrom,&box);box.left+=px(8);box.right-=px(8);box.top+=px(22);batchText(draw->nmcd.hdc,draw->nmcd.hdr.idFrom==BR_RULES?L"添加第一条规则\n组合替换、编号或拍摄时间":L"拖入文件或文件夹，开始整理名称",box,UI_FAINT,nullptr,DT_CENTER|DT_WORDBREAK);return CDRF_DODEFAULT;}
    if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT)return CDRF_NOTIFYSUBITEMDRAW;
    if(draw->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT|CDDS_SUBITEM)){
        bool selected=ListView_GetItemState(draw->nmcd.hdr.hwndFrom,(int)draw->nmcd.dwItemSpec,LVIS_SELECTED)!=0;
        draw->clrText=UI_INK_SOFT;draw->clrTextBk=selected?uiAccentWash(glassAccentColor(),11):((draw->nmcd.dwItemSpec%2)?UI_SURFACE_ALT:UI_SURFACE);
        if(draw->nmcd.hdr.idFrom==BR_FILES&&draw->iSubItem==1)draw->clrText=UI_INK;
        if(draw->nmcd.hdr.idFrom==BR_FILES&&draw->iSubItem==2){wchar_t state[256]={};ListView_GetItemText(draw->nmcd.hdr.hwndFrom,(int)draw->nmcd.dwItemSpec,2,state,256);draw->clrText=!wcscmp(state,L"可重命名")?RGB(30,140,98):!wcscmp(state,L"未变化")?UI_FAINT:UI_ACCENT_WARM;}
        return CDRF_NEWFONT;
    }return CDRF_DODEFAULT;
}
std::vector<BYTE> batchEncode() const {
    std::vector<BYTE> bytes;ePut32(bytes,0x32505242);ePut32(bytes,(DWORD)ruleStack.size());
    for(const auto&step:ruleStack){ePut32(bytes,(DWORD)step.kind);ePut32(bytes,step.enabled?1:0);ePut32(bytes,step.skipExtension?1:0);ePutText(bytes,step.text);ePutText(bytes,ever::serialize(step.rule));ePut32(bytes,(DWORD)step.options.size());for(const auto&kv:step.options){ePutText(bytes,kv.first);ePutText(bytes,kv.second);}}
    ePut32(bytes,eHash(bytes,bytes.size()));return bytes;
}
static std::vector<batch::Step> batchDecode(const std::vector<BYTE>&bytes){
    if(bytes.size()<12||bytes.size()>16*1024*1024)throw std::runtime_error("Invalid rule set size");
    size_t end=bytes.size()-4,checkAt=end;if(eGet32(bytes,checkAt)!=eHash(bytes,end))throw std::runtime_error("Invalid rule set checksum");
    size_t at=0;DWORD format=eGet32(bytes,at);if(format!=0x31505242&&format!=0x32505242)throw std::runtime_error("Invalid rule set format");DWORD count=eGet32(bytes,at);if(count>128)throw std::runtime_error("Too many rules");
    std::vector<batch::Step> steps;
    for(DWORD i=0;i<count;i++){batch::Step step;DWORD kind=eGet32(bytes,at),enabled=eGet32(bytes,at),skip=eGet32(bytes,at);if(kind>(format==0x31505242?3:4)||enabled>1||skip>1)throw std::runtime_error("Invalid rule flags");step.kind=(batch::Kind)kind;step.enabled=enabled!=0;step.skipExtension=skip!=0;step.text=eGetText(bytes,at);std::wstring error;if(!ever::deserialize(eGetText(bytes,at),step.rule,error))throw std::runtime_error("Invalid rule payload");if(format==0x32505242){DWORD options=eGet32(bytes,at);if(options>256)throw std::runtime_error("Too many options");for(DWORD n=0;n<options;n++){auto key=eGetText(bytes,at),value=eGetText(bytes,at);if(key.size()>128||step.options.count(key))throw std::runtime_error("Invalid option key");step.options[key]=value;}}steps.push_back(step);}
    if(at!=end)throw std::runtime_error("Unexpected rule data");return steps;
}
bool batchWriteRules(const std::wstring&path){auto old=eRead(path);if(old.size()>=4&&old[0]=='B'&&old[1]=='R'&&old[2]=='P'&&old[3]=='1'&&attrs(path+L".v1.bak")==INVALID_FILE_ATTRIBUTES)if(!CopyFileW(path.c_str(),(path+L".v1.bak").c_str(),TRUE))return false;return eAtomic(path,batchEncode());}
void batchStore(){if(!batchWriteRules(eHome()+L"batch-last-rules.bin"))warn(L"规则尚未保存到磁盘，本窗口中的预览仍然保留。");}
int batchSelection()const{return ListView_GetNextItem(ctl(BR_RULES),-1,LVNI_SELECTED);}
void batchList(int selected=-1){
    batchUpdating=true;ListView_DeleteAllItems(ctl(BR_RULES));
    for(size_t i=0;i<ruleStack.size();i++){std::wstring label=std::to_wstring(i+1)+L". "+batchRuleSummary(ruleStack[i]);LVITEMW item={};item.mask=LVIF_TEXT;item.iItem=(int)i;item.pszText=(LPWSTR)label.c_str();ListView_InsertItem(ctl(BR_RULES),&item);ListView_SetCheckState(ctl(BR_RULES),(int)i,ruleStack[i].enabled);if((int)i==selected)ListView_SetItemState(ctl(BR_RULES),(int)i,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);}
    batchUpdating=false;
}
void batchFiles(){
    if(!ctl(BR_FILES)||!editor)return;
    ListView_DeleteAllItems(ctl(BR_FILES));auto names=eNameLines(text());size_t count=std::min<size_t>(items.size(),1000);
    std::vector<std::wstring> targets;targets.reserve(names.size());for(size_t i=0;i<names.size()&&i<items.size();i++)targets.push_back(names[i]+(hidden&&i<extensions.size()?extensions[i]:L""));
    std::map<std::wstring,int> duplicates;for(size_t i=0;i<targets.size();i++){std::wstring key=parent(items[i].path)+targets[i];std::transform(key.begin(),key.end(),key.begin(),towlower);duplicates[key]++;}
    for(size_t i=0;i<count;i++){
        LVITEMW item={};item.mask=LVIF_TEXT;item.iItem=(int)i;item.pszText=(LPWSTR)items[i].name.c_str();ListView_InsertItem(ctl(BR_FILES),&item);
        std::wstring target=i<targets.size()?targets[i]:L"（缺少对应行）",state=L"未变化",reason;
        if(i<names.size()&&(names[i].empty()||(!hidden&&i<extensions.size()&&!extensions[i].empty()&&names[i]==extensions[i])))state=L"名称为空，请补充";
        else if(target!=items[i].name){state=L"可重命名";if(!validName(target,reason))state=reason;else{std::wstring key=parent(items[i].path)+target;std::transform(key.begin(),key.end(),key.begin(),towlower);if(duplicates[key]>1)state=L"名称重复";else if(attrs(parent(items[i].path)+target)!=INVALID_FILE_ATTRIBUTES){bool selected=false;for(const auto&entry:items)if(same(entry.path,parent(items[i].path)+target)){selected=true;break;}if(!selected)state=L"目标已存在";}}}
        ListView_SetItemText(ctl(BR_FILES),(int)i,1,(LPWSTR)target.c_str());ListView_SetItemText(ctl(BR_FILES),(int)i,2,(LPWSTR)state.c_str());
    }
    std::wstring title=L"文件预览 · "+std::to_wstring(items.size())+L"个项目";if(items.size()>1000)title+=L"（表格显示前1000项，下方编辑区包含全部名称）";SetWindowTextW(ctl(BR_HEADING),title.c_str());
}
bool batchPreview(bool confirmManual=true){
    if(batchSimple){batchFiles();return true;}
    if(items.empty()){lastRulePreview.clear();batchFiles();return true;}
    if(confirmManual&&!lastRulePreview.empty()&&text()!=lastRulePreview&&MessageBoxW(window,L"重新预览会以原始名称重新计算规则，并替换当前手工编辑的名称。是否继续？",L"批量重命名",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return false;
    std::vector<batch::Item> inputs;for(const auto&entry:items)inputs.push_back({entry.path,entry.name,entry.folder});std::vector<std::wstring> names;std::wstring error;
    std::vector<std::wstring>exts;if(!batch::preview(ruleStack,inputs,names,error,&exts)){warn(error);return false;}
    std::vector<std::wstring> display=names;
    for(size_t i=0;i<names.size();i++)if(hidden)display[i].resize(display[i].size()-exts[i].size());
    textAndExtensionMode(display,hidden,exts);lastRulePreview=text();batchFiles();status(L"已按顺序预览 "+std::to_wstring(ruleStack.size())+L" 条规则；尚未修改真实文件。" );return true;
}
#include "renamer_form.inc"
bool batchForm(batch::Step&step){
    if(step.kind==batch::Kind::Advanced)return batchAdvancedForm(step);
    if(step.kind==batch::Kind::Rule&&step.text.compare(0,3,L"旧记忆")==0){bool remember=false;if(step.rule.mode==ever::Mode::FindReplace)return replacementForm(step.rule,remember);if(step.rule.mode==ever::Mode::NamingTemplate)return namingForm(step.rule,remember);return ruleForm(step.rule,remember);}
    std::vector<std::wstring> types={L"插入文本",L"删除字符",L"文本替换",L"正则替换",L"转为大写",L"转为小写",L"清理空白",L"编号 / 元数据模板",L"修改扩展名",L"移除指定字符"};
    int type=step.kind==batch::Kind::Clear?1:step.kind==batch::Kind::Extension?8:step.kind==batch::Kind::Strip?9:step.rule.mode==ever::Mode::NamingTemplate?7:step.rule.mode==ever::Mode::FindReplace?(step.rule.regex?3:2):step.rule.operation==ever::Operation::Delete?1:step.rule.operation==ever::Operation::Convert?(step.rule.conversion==ever::Conversion::Upper?4:step.rule.conversion==ever::Conversion::Lower?5:6):0;
    std::wstring find,replace;const auto &pairs=step.rule.replacements;size_t start=pairs.compare(0,4,L"|=\u241e\n")==0?4:0;wchar_t divider=start?L'\u241e':L'|';size_t delimiter=pairs.find(divider,start);if(delimiter!=std::wstring::npos){find=pairs.substr(start,delimiter-start);replace=pairs.substr(delimiter+1);}
    EForm form;form.fields={{101,L"规则类型",types[type],4,types},{102,L"插入 / 扩展名 / 移除字符",step.kind==batch::Kind::Rule?step.rule.text:step.text},{103,L"插入位置",step.rule.position==ever::Position::Suffix?L"末尾":step.rule.position==ever::Position::Index?L"指定位置":L"开头",4,{L"开头",L"末尾",L"指定位置"}},{104,L"位置（1表示开头，-1表示末尾）",std::to_wstring(step.rule.index)},{105,L"删除字符数",std::to_wstring(step.rule.count)},{106,L"查找内容",find},{107,L"替换内容",replace},{108,L"命名格式",step.rule.namingPattern},{109,L"编号起始",step.rule.sequenceStart},{110,L"编号步长",std::to_wstring(step.rule.sequenceStep)},{111,L"编号位数",std::to_wstring(step.rule.sequenceWidth>0?step.rule.sequenceWidth:3)},{112,L"保留扩展名，不参与本条规则",step.skipExtension?L"1":L"0",1},{113,L"删除方式",step.kind==batch::Kind::Clear?L"清空名称":L"按位置删除",4,{L"按位置删除",L"清空名称"}}};
    for(auto&field:form.fields){if(field.id==108)field.help=L"例如：照片-{taken:yyyyMMdd}-{num}。支持{name}、{num}、{parent}、{taken}、{created}、{modified}、{now}。扩展名会自动保留。";if(field.id==106||field.id==107)field.help=L"内容中的管道符可用已有“改部分”表单配置；正则替换支持.NET分组。";}
    form.changed=[](EForm&f){auto t=f.get(101);bool deletion=t==L"删除字符"&&f.get(113)!=L"清空名称";for(auto&field:f.fields){int id=field.id;field.visible=id==101||id==112||(id==113?t==L"删除字符":id==102?(t==L"插入文本"||t==L"修改扩展名"||t==L"移除指定字符"):id==103?t==L"插入文本":id==104?deletion||(t==L"插入文本"&&f.get(103)==L"指定位置"):id==105?deletion:id==106||id==107?t==L"文本替换"||t==L"正则替换":id>=108&&id<=111?t==L"编号 / 元数据模板":false);}};
    if(!form.run(window,dpi,L"添加 / 编辑规则"))return false;
    batch::Step result;result.enabled=step.enabled;result.skipExtension=form.value(112)==L"1";auto t=form.value(101);auto&r=result.rule;
    if(t==L"删除字符"&&form.value(113)==L"清空名称")result.kind=batch::Kind::Clear;
    else if(t==L"修改扩展名"||t==L"移除指定字符"){result.kind=t==L"修改扩展名"?batch::Kind::Extension:batch::Kind::Strip;result.text=form.value(102);}
    else if(t==L"文本替换"||t==L"正则替换"){
        if(form.value(106).empty()){warn(L"查找内容不能为空。");return false;}if(form.value(106).find(L'\u241e')!=std::wstring::npos||form.value(107).find(L'\u241e')!=std::wstring::npos){warn(L"替换内容含保留的分隔字符。");return false;}r.mode=ever::Mode::FindReplace;r.regex=t==L"正则替换";r.replacements=L"|=\u241e\n"+form.value(106)+L"\u241e"+form.value(107);r.escapeFind=false;r.escapeReplacement=false;
    }else if(t==L"编号 / 元数据模板"){
        r.mode=ever::Mode::NamingTemplate;r.namingPattern=form.value(108);r.sequenceStart=form.value(109);int stride=0;if(!eInteger(form.value(110),stride)||stride<1||!eInteger(form.value(111),r.sequenceWidth)||r.sequenceWidth<1||r.sequenceWidth>18){warn(L"编号步长必须为正整数，位数为1—18。");return false;}r.sequenceStep=stride;
    }else if(t==L"转为大写"||t==L"转为小写"||t==L"清理空白"){r.operation=ever::Operation::Convert;r.conversion=t==L"转为大写"?ever::Conversion::Upper:t==L"转为小写"?ever::Conversion::Lower:ever::Conversion::RemoveWhitespace;}
    else{r.operation=t==L"删除字符"?ever::Operation::Delete:ever::Operation::Add;r.text=form.value(102);auto position=form.value(103);r.position=position==L"末尾"?ever::Position::Suffix:position==L"指定位置"?ever::Position::Index:ever::Position::Prefix;if((r.operation==ever::Operation::Delete||r.position==ever::Position::Index)&&!eInteger(form.value(104),r.index)){warn(L"位置必须为整数。");return false;}if(r.operation==ever::Operation::Delete){r.position=ever::Position::Index;if(!eInteger(form.value(105),r.count)||r.count<1){warn(L"删除数量必须为正整数。");return false;}}}
    step=result;return true;
}
void batchMode(bool simple){
    if(simple==batchSimple)return;
    if(!simple&&!protectDraft())return;
    batchSimple=simple;eSaveSetting(L"BatchSimple",simple?L"1":L"0");
    if(!simple)batchPreview(false);layout();batchFiles();
}
bool batchQuickApply(const batch::Step&step){
    if(items.empty()){warn(L"请先添加文件或文件夹。");return false;}
    if(step.kind==batch::Kind::Clear&&!hidden)toggle(true);
    std::vector<std::wstring> current;if(!names(current))return false;std::vector<batch::Item>inputs;
    for(size_t i=0;i<items.size();i++)inputs.push_back({items[i].path,current[i],hidden||items[i].folder});
    std::vector<std::wstring>out;std::wstring error;if(!batch::preview({step},inputs,out,error)){warn(error);return false;}
    setText(eJoin(out));batchFiles();status(L"简易处理已更新预览，可继续处理或撤销；点击执行后才修改文件。");return true;
}
void batchQuick(int id){
    if(id==BR_QUNDO){sci(SCI_UNDO);return;}if(id==BR_QREDO){sci(SCI_REDO);return;}
    if(id==BR_QRESTORE){restoreNames();batchFiles();return;}
    batch::Step step;
    if(id==BR_QUPPER||id==BR_QLOWER||id==BR_QSPACE){step.rule.operation=ever::Operation::Convert;step.rule.conversion=id==BR_QUPPER?ever::Conversion::Upper:id==BR_QLOWER?ever::Conversion::Lower:ever::Conversion::RemoveWhitespace;}
    else if(id==BR_QNUMBER){step.kind=batch::Kind::Advanced;step.options={{L"type",L"Serialize"},{L"where",L"suffix"},{L"start",L"1"},{L"width",L"3"},{L"prefix",L"-"}};if(!batchAdvancedForm(step))return;}
    else if(id==BR_QADD){EForm f;f.fields={{101,L"添加的文字",L""},{102,L"添加到",L"开头",4,{L"开头",L"末尾"}}};if(!f.run(window,dpi,L"简易处理 · 添加文字"))return;step.rule.text=f.value(101);step.rule.position=f.value(102)==L"末尾"?ever::Position::Suffix:ever::Position::Prefix;}
    else if(id==BR_QDELETE){EForm f;f.fields={{101,L"删除方式",L"按位置删除",4,{L"按位置删除",L"清空名称"}},{102,L"开始位置（1为开头，-1为末尾）",L"1"},{103,L"删除字符数",L"1"}};f.changed=[](EForm&p){for(auto&field:p.fields)if(field.id!=101)field.visible=p.get(101)!=L"清空名称";};if(!f.run(window,dpi,L"简易处理 · 删除 / 清空"))return;if(f.value(101)==L"清空名称")step.kind=batch::Kind::Clear;else{step.rule.operation=ever::Operation::Delete;step.rule.position=ever::Position::Index;if(!eInteger(f.value(102),step.rule.index)||!eInteger(f.value(103),step.rule.count)||step.rule.count<1){warn(L"位置须为整数，删除数量须为正整数。");return;}}}
    else if(id==BR_QREPLACE){EForm f;f.fields={{101,L"查找的文字",L""},{102,L"替换为（留空即删除）",L""}};if(!f.run(window,dpi,L"简易处理 · 替换文字"))return;if(f.value(101).empty()){warn(L"查找文字不能为空。");return;}if(f.value(101).find(L'\u241e')!=std::wstring::npos||f.value(102).find(L'\u241e')!=std::wstring::npos){warn(L"文字包含保留分隔字符。");return;}step.rule.mode=ever::Mode::FindReplace;step.rule.regex=false;step.rule.ignoreCase=false;step.rule.escapeFind=false;step.rule.escapeReplacement=false;step.rule.replacements=L"|=\u241e\n"+f.value(101)+L"\u241e"+f.value(102);}
    batchQuickApply(step);
}
std::vector<std::wstring> batchPresets(){
    std::vector<std::wstring>names;WIN32_FIND_DATAW data={};HANDLE find=FindFirstFileW((ePresetHome()+L"*.llrules").c_str(),&data);if(find==INVALID_HANDLE_VALUE)return names;
    do{if(!(data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){std::wstring n=data.cFileName;names.push_back(n.substr(0,n.size()-8));}}while(FindNextFileW(find,&data));FindClose(find);std::sort(names.begin(),names.end());return names;
}
bool batchSavePreset(const std::wstring&name){
    std::wstring reason;if(name.size()>80||!validName(name,reason)){warn(L"预设名称无效，请使用80字以内的普通名称。");return false;}
    auto home=ePresetHome();std::wstring directory=home.substr(0,home.size()-1);
    if(!CreateDirectoryW(directory.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS){warn(L"无法在安装目录创建 RenamePresets，请检查安装目录写入权限。");return false;}
    if(!batchWriteRules(home+name+L".llrules")){warn(L"预设保存失败，请检查安装目录写入权限。");return false;}status(L"预设已保存到安装目录的 RenamePresets："+name);return true;
}
void batchFolderMenu(){HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"选择多个文件夹（重命名文件夹本身）…");AppendMenuW(menu,MF_STRING,2,L"选择父目录，批量添加直接子文件夹…");RECT r;GetWindowRect(ctl(EE_FOLDER),&r);int id=TrackPopupMenu(menu,TPM_RETURNCMD,r.left,r.bottom,0,window,nullptr);DestroyMenu(menu);if(id)choose(true,id==2);}
void batchCommand(int id){
    int selected=batchSelection();
    if(id==BR_SIMPLE||id==BR_ADVANCED){batchMode(id==BR_SIMPLE);return;}
    if(id>=BR_QADD&&id<=BR_QREDO){batchQuick(id);return;}
    if(id==BR_PREVIEW){batchPreview();return;}
    if(id==BR_SAVE||id==BR_LOAD){
        if(id==BR_SAVE){std::wstring name;if(!eAsk(window,dpi,L"保存预设 · 安装目录",L"预设名称",name)||name.empty())return;if(attrs(ePresetHome()+name+L".llrules")!=INVALID_FILE_ATTRIBUTES&&MessageBoxW(window,L"同名预设已存在，是否替换？",L"保存预设",MB_YESNO|MB_DEFBUTTON2)!=IDYES)return;batchSavePreset(name);}
        else{auto names=batchPresets();if(names.empty()){warn(L"安装目录的 RenamePresets 中还没有预设。");return;}HMENU menu=CreatePopupMenu();for(size_t i=0;i<names.size();i++){std::wstring label=names[i];size_t at=0;while((at=label.find(L'&',at))!=label.npos){label.insert(at,1,L'&');at+=2;}AppendMenuW(menu,MF_STRING,i+1,label.c_str());}RECT r;GetWindowRect(ctl(BR_LOAD),&r);int choice=TrackPopupMenu(menu,TPM_RETURNCMD,r.left,r.bottom,0,window,nullptr);DestroyMenu(menu);if(choice>0&&choice<=(int)names.size()){auto candidate=batchDecode(eRead(ePresetHome()+names[choice-1]+L".llrules"));if(!protectDraft())return;ruleStack.swap(candidate);batchStore();batchList();batchPreview(false);}}return;
    }
    if(id==BR_LEGACY){auto templates=eTemplates();if(templates.empty()){warn(L"未找到旧记忆模板。已有模板与后悔药仍使用原数据目录。");return;}HMENU menu=CreatePopupMenu();for(size_t i=0;i<templates.size();i++)AppendMenuW(menu,MF_STRING,i+1,templates[i].name.c_str());POINT p;GetCursorPos(&p);int choice=TrackPopupMenu(menu,TPM_RETURNCMD,p.x,p.y,0,window,nullptr);DestroyMenu(menu);if(choice<1||choice>(int)templates.size())return;batch::Step step;step.rule=templates[choice-1].rule;step.text=L"旧记忆："+templates[choice-1].name;ruleStack.push_back(step);selected=(int)ruleStack.size()-1;}
    else if(id==BR_ADD){if(ruleStack.size()>=128){warn(L"最多添加128条规则。");return;}batch::Step step;step.kind=batch::Kind::Advanced;if(!batchForm(step))return;ruleStack.push_back(step);selected=(int)ruleStack.size()-1;}
    else if(id==BR_CLEAR)ruleStack.clear();
    else if(selected<0||selected>=(int)ruleStack.size()){warn(L"请先选择一条规则。");return;}
    else if(id==BR_EDIT){auto step=ruleStack[selected];if(!batchForm(step))return;ruleStack[selected]=step;}
    else if(id==BR_DELETE){ruleStack.erase(ruleStack.begin()+selected);if(selected>=(int)ruleStack.size())selected--;}
    else if(id==BR_UP&&selected>0){std::swap(ruleStack[selected],ruleStack[selected-1]);selected--;}
    else if(id==BR_DOWN&&selected+1<(int)ruleStack.size()){std::swap(ruleStack[selected],ruleStack[selected+1]);selected++;}
    batchStore();batchList(selected);batchPreview();
}
void batchControls(){
    batchSimple=eSetting(L"BatchSimple",L"1")==L"1";
    auto label=[&](int id,const wchar_t*text){HWND h=CreateWindowW(L"STATIC",text,WS_CHILD|WS_VISIBLE,0,0,0,0,window,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);};
    label(BR_HEADING,L"文件预览 · 原名称 → 新名称");label(BR_MANUAL,L"手工编辑预览名称（每行对应一个项目；Ctrl+Z撤销）");
    HWND rules=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_NOCOLUMNHEADER,0,0,0,0,window,(HMENU)BR_RULES,GetModuleHandleW(nullptr),nullptr);
    ListView_SetExtendedListViewStyle(rules,LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);LVCOLUMNW column={};column.mask=LVCF_TEXT|LVCF_WIDTH;column.pszText=(LPWSTR)L"规则队列 · 从上到下执行";column.cx=px(240);ListView_InsertColumn(rules,0,&column);SendMessageW(rules,WM_SETFONT,(WPARAM)font,TRUE);
    HWND files=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_NOCOLUMNHEADER,0,0,0,0,window,(HMENU)BR_FILES,GetModuleHandleW(nullptr),nullptr);ListView_SetExtendedListViewStyle(files,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    const wchar_t*columns[]={L"原名称",L"预览名称",L"检查"};for(int i=0;i<3;i++){column.pszText=(LPWSTR)columns[i];column.cx=px(i==2?105:245);ListView_InsertColumn(files,i,&column);}SendMessageW(files,WM_SETFONT,(WPARAM)font,TRUE);
    const int ids[]={BR_ADD,BR_EDIT,BR_UP,BR_DOWN,BR_DELETE,BR_CLEAR,BR_PREVIEW,BR_SAVE,BR_LOAD,BR_LEGACY};const wchar_t*labels[]={L"新增规则",L"编辑",L"上移",L"下移",L"删除",L"清空",L"重新预览",L"保存规则集",L"载入规则集",L"导入旧记忆"};
    for(int i=0;i<10;i++){HWND h=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,window,(HMENU)(INT_PTR)ids[i],GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);eTrackHover(h,&hot);}
    const wchar_t*quickLabels[]={L"简易处理",L"规则队列",L"添加文字",L"删除 / 清空",L"替换文字",L"编号 / 文件信息",L"转为大写",L"转为小写",L"清理空白",L"还原名称",L"撤销",L"重做"};
    for(int i=0;i<12;i++){HWND h=CreateWindowW(L"BUTTON",quickLabels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,window,(HMENU)(INT_PTR)(BR_SIMPLE+i),GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);eTrackHover(h,&hot);}
    SetWindowTextW(ctl(EE_OK),L"执行重命名  →");SetWindowTextW(ctl(EE_OTHER),L"更多工具  ···");SetWindowTextW(ctl(EE_ADD),L"＋ 添加文件");
    batchTitleFont=CreateFontW(-px(23),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    batchRowImages=ImageList_Create(1,px(36),ILC_COLOR32,1,1);ListView_SetImageList(rules,batchRowImages,LVSIL_SMALL);ListView_SetImageList(files,batchRowImages,LVSIL_SMALL);
    ListView_SetBkColor(rules,UI_SURFACE);ListView_SetTextBkColor(rules,CLR_NONE);ListView_SetBkColor(files,UI_SURFACE);ListView_SetTextBkColor(files,CLR_NONE);
    SetWindowLongPtrW(editor,GWL_EXSTYLE,GetWindowLongPtrW(editor,GWL_EXSTYLE)&~WS_EX_CLIENTEDGE);SetWindowPos(editor,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    sci(SCI_SETCARETLINEBACK,UI_SURFACE_ALT);sci(SCI_SETEXTRAASCENT,px(6));sci(SCI_SETEXTRADESCENT,px(5));
    const int hiddenButtons[]={EE_ZOOMIN,EE_ZOOMOUT,EE_COPY,EE_SPEAK,EE_SEARCH,EE_UNDO,EE_REDO,EE_RULE,EE_NAMING,EE_EXT,EE_LINK,BR_HEADING,BR_MANUAL};
    for(int id:hiddenButtons)ShowWindow(ctl(id),SW_HIDE);
    if(!batchLegacyAction)try{auto bytes=eRead(eHome()+L"batch-last-rules.bin");if(!bytes.empty())ruleStack=batchDecode(bytes);}catch(const std::exception&){warn(L"上次规则队列损坏，未覆盖原文件。可载入规则集重新配置。");}batchList();
}
void batchLayout(int width,int height){
    int margin=px(22),left=px(270),right=margin+left+px(18),bottom=height-px(91),top=px(110),available=width-right-margin;
    batchRuleCard={margin,top,margin+left,bottom};int tableHeight=(bottom-top)*56/100;
    batchTableCard={right,top,width-margin,top+tableHeight};batchEditCard={right,top+tableHeight+px(16),width-margin,bottom};
    auto move=[&](int id,int x,int y,int w,int h){MoveWindow(ctl(id),x,y,w,h,TRUE);};
    int toolbar=width-px(500);const int topIds[]={EE_ADD,EE_FOLDER,EE_READ,EE_OTHER};const int topWidths[]={112,112,112,110};
    for(int i=0;i<4;i++){move(topIds[i],toolbar,px(28),px(topWidths[i]),px(36));toolbar+=px(topWidths[i]+8);}
    int x=margin+px(16);move(BR_SIMPLE,x,top+px(44),px(114),px(30));move(BR_ADVANCED,x+px(124),top+px(44),px(114),px(30));
    EnableWindow(ctl(BR_SIMPLE),!batchSimple);EnableWindow(ctl(BR_ADVANCED),batchSimple);
    for(int id=BR_QADD;id<=BR_QREDO;id++){int i=id-BR_QADD;ShowWindow(ctl(id),batchSimple?SW_SHOW:SW_HIDE);move(id,x+(i%2)*px(124),top+px(124)+(i/2)*px(39),px(114),px(32));}
    for(int id:{BR_RULES,BR_ADD,BR_EDIT,BR_UP,BR_DOWN,BR_DELETE,BR_CLEAR,BR_PREVIEW,BR_LEGACY,BR_SAVE,BR_LOAD})ShowWindow(ctl(id),batchSimple?SW_HIDE:SW_SHOW);
    int y=top+px(120);move(BR_ADD,x,y,px(150),px(36));move(BR_EDIT,x+px(160),y,px(78),px(36));
    y+=px(48);const int order[]={BR_UP,BR_DOWN,BR_DELETE,BR_CLEAR};for(int i=0;i<4;i++)move(order[i],x+i*px(61),y,px(55),px(29));
    y+=px(39);int listBottom=bottom-px(114);move(BR_RULES,x,y,left-px(32),std::max(px(20),listBottom-y));ListView_SetColumnWidth(ctl(BR_RULES),0,left-px(34));
    const int presets[]={BR_PREVIEW,BR_LEGACY,BR_SAVE,BR_LOAD};const wchar_t*names[]={L"刷新预览",L"旧版记忆",L"保存预设",L"载入预设"};
    for(int i=0;i<4;i++){SetWindowTextW(ctl(presets[i]),names[i]);move(presets[i],x+(i%2)*px(124),bottom-px(96)+(i/2)*px(40),px(114),px(32));}
    int col=(available-px(36)-px(116))/2;move(BR_FILES,right+px(18),top+px(88),available-px(36),std::max(px(24),tableHeight-px(104)));
    ListView_SetColumnWidth(ctl(BR_FILES),0,col);ListView_SetColumnWidth(ctl(BR_FILES),1,col);ListView_SetColumnWidth(ctl(BR_FILES),2,px(108));
    move(EE_FIND,width-margin-px(116),batchEditCard.top+px(12),px(98),px(29));SetWindowTextW(ctl(EE_FIND),L"查找替换");
    MoveWindow(editor,right+px(16),batchEditCard.top+px(54),available-px(32),std::max<int>(px(30),bottom-batchEditCard.top-px(70)),TRUE);
    move(EE_OK,width-margin-px(174),height-px(64),px(174),px(42));move(EE_STATUS,px(26),height-px(68),width-px(240),px(52));
    InvalidateRect(window,nullptr,FALSE);
}
void batchNotify(NMHDR*header){
    if(header->idFrom==BR_RULES&&!batchUpdating){
        if(header->code==NM_DBLCLK){batchCommand(BR_EDIT);return;}
        if(header->code==LVN_ITEMCHANGED){auto change=(NMLISTVIEW*)header;if((change->uChanged&LVIF_STATE)&&((change->uOldState^change->uNewState)&LVIS_STATEIMAGEMASK)&&change->iItem>=0&&change->iItem<(int)ruleStack.size()){ruleStack[change->iItem].enabled=ListView_GetCheckState(ctl(BR_RULES),change->iItem)!=0;batchStore();batchPreview();}}
    }
    if(header->idFrom==BR_FILES&&header->code==NM_CLICK){int index=ListView_GetNextItem(ctl(BR_FILES),-1,LVNI_SELECTED);if(index>=0)sci(SCI_GOTOLINE,index);}
}
