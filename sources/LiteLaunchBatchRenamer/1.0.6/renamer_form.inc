// Members of EverWindow. One rule window; identifiers never appear as menu labels.
#include "renamer_picker.inc"
bool batchAdvancedForm(batch::Step&step){
 EForm form;std::wstring type=step.options.count(L"type")?step.options.at(L"type"):L"Insert";
 std::map<std::wstring,std::map<std::wstring,std::wstring>>drafts;drafts[type]=step.options;
 std::map<int,std::wstring>keys;std::map<int,std::vector<std::pair<std::wstring,std::wstring>>>choiceValues;std::vector<int>metadataTargets;
 std::vector<std::wstring>typeLabels;for(const auto&entry:batch::ruleTypes())typeLabels.push_back(entry.second);
 // Read old templates into the new field selectors without asking for tag syntax.
 if(type==L"Metadata"&&!drafts[type].count(L"metaTag")){
  auto&o=drafts[type];auto pattern=o.count(L"pattern")?o[L"pattern"]:L":File_Name:";std::vector<std::pair<size_t,std::wstring>>found;
  for(const auto&row:batchMetaRows()){auto token=L":"+row[1]+L":";size_t at=pattern.find(token);if(at!=pattern.npos)found.push_back({at,row[1]});}std::sort(found.begin(),found.end());
  if(!found.empty()){auto first=found[0];o[L"metaTag"]=first.second;o[L"prefix"]=pattern.substr(0,first.first);size_t end=first.first+first.second.size()+2;if(found.size()>1){auto last=found[1];o[L"metaTag2"]=last.second;o[L"separator"]=pattern.substr(end,last.first-end);end=last.first+last.second.size()+2;}o[L"suffix"]=pattern.substr(end);}else o[L"metaTag"]=L"File_Name";
 }
 auto selected=[&](EForm&p,int id){auto value=p.window?p.get(id):p.value(id);for(const auto&pair:choiceValues[id])if(pair.second==value)return pair.first;return value;};
 auto save=[&](EForm&p){auto&o=drafts[type];for(const auto&field:p.fields){auto key=keys.find(field.id);if(key==keys.end())continue;auto value=!p.window?field.value:field.type==1?(p.check(field.id)?L"1":L"0"):field.type>=3?selected(p,field.id):p.get(field.id);if(field.type>=3)for(const auto&pair:choiceValues[field.id])if(pair.second==value){value=pair.first;break;}if(std::find(metadataTargets.begin(),metadataTargets.end(),field.id)!=metadataTargets.end())value=batchTemplateText(value,true);o[key->second]=value;}o[L"type"]=type;o[L"label"]=batch::ruleLabel(type);o[L"missing"]=L"empty";o[L"sanitize"]=L"1";o[L"uiMore"]=(p.window?p.check(600):p.value(600)==L"1")?L"1":L"0";
  if(type==L"PascalScript")o[L"compileOnly"]=L"0";
  if(type==L"Strip"){o[L"sets"].clear();for(const auto&pair:std::vector<std::pair<std::wstring,std::wstring>>{{L"uiEnglish",L"english"},{L"uiDigits",L"digits"},{L"uiSymbols",L"symbols"},{L"uiBrackets",L"brackets"}})if(o[pair.first]==L"1")o[L"sets"]+=pair.second+L",";}
  if(type==L"Metadata"){o[L"pattern"]=o[L"prefix"]+L":"+o[L"metaTag"]+L":";if(!o[L"metaTag2"].empty())o[L"pattern"]+=o[L"separator"]+L":"+o[L"metaTag2"]+L":";o[L"pattern"]+=o[L"suffix"];}
 };
 std::function<std::vector<EField>()>build=[&](){
  EForm f;f.fields={{100,L"处理方式",batch::ruleLabel(type),4,typeLabels}};auto o=drafts[type];keys.clear();choiceValues.clear();metadataTargets.clear();int next=101;
 auto add=[&](const wchar_t*key,const wchar_t*label,const wchar_t*value=L"",int style=0,std::vector<std::wstring>values=std::vector<std::wstring>(),bool metadata=false){int id=next++;std::wstring v=o.count(key)?o[key]:value;std::vector<std::wstring>labels;
  if(style>=3){bool matched=false;for(const auto&entry:values){size_t at=entry.find(L" · ");std::wstring code=at==entry.npos?entry:entry.substr(0,at),label=at==entry.npos?entry:entry.substr(at+3);choiceValues[id].push_back({code,label});labels.push_back(label);if(code==v){v=label;matched=true;}}if(!matched&&!v.empty()){choiceValues[id].insert(choiceValues[id].begin(),{v,L"原来的设置"});labels.insert(labels.begin(),L"原来的设置");v=L"原来的设置";}}
  if(metadata){v=batchTemplateText(v,false);metadataTargets.push_back(id);}
  f.fields.push_back({id,label,v,style,labels});keys[id]=key;return id;};
 auto edit=[&](const wchar_t*k,const wchar_t*l,const wchar_t*v=L"",bool meta=false){return add(k,l,v,0,{},meta);};
 auto check=[&](const wchar_t*k,const wchar_t*l,bool v=false){return add(k,l,v?L"1":L"0",1);};
 auto combo=[&](const wchar_t*k,const wchar_t*l,const wchar_t*v,std::vector<std::wstring>c){return add(k,l,v,4,c);};
 auto multi=[&](const wchar_t*k,const wchar_t*l,const wchar_t*v=L""){return add(k,l,v,2);};
 auto place=[&](){combo(L"where",L"放在哪里",L"suffix",{L"prefix · 开头",L"suffix · 末尾",L"position · 指定位置",L"before · 定位文字前",L"after · 定位文字后",L"replace · 替换整个名称"});edit(L"position",L"插入位置（从1开始）",L"1");edit(L"anchor",L"定位文字");check(L"rtl",L"位置从右侧计数");};
 if(type==L"Insert"){edit(L"text",L"添加内容",L"",true);place();check(L"case",L"定位文字区分大小写");}
 else if(type==L"Delete"){check(L"clear",L"清空名称");combo(L"from",L"起始方式",L"position",{L"position · 指定位置",L"delimiter · 指定分隔符"});edit(L"position",L"起始位置（从1开始）",L"1");edit(L"fromText",L"起始分隔符");combo(L"until",L"结束方式",L"count",{L"count · 删除指定数量",L"delimiter · 指定分隔符",L"end · 直到末尾"});edit(L"count",L"删除字符数",L"1");edit(L"untilText",L"结束分隔符");check(L"keepDelimiters",L"保留起止分隔符");check(L"rtl",L"从右向左删除");}
 else if(type==L"Remove"||type==L"Replace"||type==L"RegEx"){
  edit(L"find",type==L"RegEx"?L"正则表达式":L"查找内容（多项以 *|* 分隔）");if(type!=L"Remove")edit(L"replace",L"替换为",L"",true);
  combo(L"occurrence",L"匹配范围",L"all",{L"all · 全部",L"first · 第一次",L"last · 最后一次"});check(L"case",L"区分大小写");check(L"whole",L"仅匹配完整单词");if(type!=L"RegEx"){check(L"wildcards",L"通配符 * ? [a-z] [!a-z]");if(type==L"Replace")check(L"sameReplacement",L"替换项不足时复用第一项");}else{check(L"multiline",L"多行模式（^ 和 $）");check(L"singleline",L"单行模式（. 匹配换行）");}}
 else if(type==L"Rearrange"){combo(L"split",L"拆分方式",L"delimiters",{L"delimiters · 分隔符列表",L"positions · 位置列表",L"exact · 顺序分隔符"});edit(L"splitBy",L"拆分依据（多项以 | 分隔）",L" - ");edit(L"pattern",L"重排格式（$0 原名，$1 首段，$-1 末段）",L"$2 - $1",true);check(L"rtl",L"分段从右向左");}
 else if(type==L"Extension"){edit(L"extension",L"新扩展名（留空即移除）",L"",true);check(L"detect",L"按文件内容自动识别扩展名");}
 else if(type==L"Strip"){edit(L"characters",L"要移除的字符");check(L"uiEnglish",L"英文字母",o[L"sets"].find(L"english")!=std::wstring::npos);check(L"uiDigits",L"数字",o[L"sets"].find(L"digits")!=std::wstring::npos);check(L"uiSymbols",L"符号",o[L"sets"].find(L"symbols")!=std::wstring::npos);check(L"uiBrackets",L"括号",o[L"sets"].find(L"brackets")!=std::wstring::npos);edit(L"ranges",L"字符编码范围");combo(L"stripWhere",L"移除位置",L"all",{L"all · 全部",L"leading · 开头",L"trailing · 末尾"});check(L"invert",L"只保留这些字符");check(L"case",L"区分大小写");}
 else if(type==L"Case"){combo(L"caseMode",L"大小写转换",L"title",{L"title · 每词首字母大写",L"lower · 小写",L"upper · 大写",L"invert · 反转",L"first · 首字母大写",L"sentence · 每句首字母大写",L"none · 不转换"});check(L"preserve",L"保留其他字母的现有大小写");edit(L"forceCase",L"强制大小写片段（逗号分隔，如 iPhone,USB）");check(L"forceWhole",L"强制片段仅匹配完整单词",true);combo(L"extensionCase",L"扩展名大小写",L"none",{L"none · 保持",L"lower · 小写",L"upper · 大写"});}
 else if(type==L"Serialize"){edit(L"start",L"从几开始",L"1");edit(L"step",L"每次增加",L"1");edit(L"width",L"编号位数",L"3");edit(L"repeat",L"每个编号重复次数",L"1");edit(L"resetEvery",L"每多少项目重置（0为不重置）",L"0");check(L"resetFolder",L"目录变化时重置");check(L"resetName",L"名称变化时重置");combo(L"system",L"编号样式",L"decimal",{L"decimal · 十进制",L"letters · 英文字母",L"roman · 罗马数字",L"music · 音符",L"chinese · 中文数字",L"alphabetic · 自定义字母表",L"numeric · 自定义数制"});edit(L"alphabet",L"自定义字符",L"01");edit(L"prefix",L"编号前面加字",L"",true);edit(L"suffix",L"编号后面加字",L"",true);place();}
 else if(type==L"Randomize"){edit(L"length",L"随机序列长度",L"8");combo(L"randomChars",L"字符集合",L"digits",{L"digits · 数字",L"letters · 英文字母",L"custom · 自定义"});edit(L"alphabet",L"自定义字符集",L"0123456789abcdef");check(L"unique",L"避免重复",true);edit(L"seed",L"随机种子",L"1");place();}
 else if(type==L"Padding"){combo(L"padMode",L"填充方式",L"numbers",{L"numbers · 对名称中的数字补零",L"text · 填充整个名称",L"remove · 移除数字前导零"});edit(L"width",L"目标长度 / 数字位数",L"3");edit(L"padText",L"填充文字",L"0");combo(L"side",L"填充方向",L"left",{L"left · 左侧",L"right · 右侧"});}
 else if(type==L"CleanUp"){edit(L"brackets",L"删除括号及其中内容，如 ()[]{}",L"()[]{}");edit(L"spaces",L"替换为空格的字符",L"._");check(L"keepNumbers",L"保留数字间的分隔符",true);check(L"unicodeSpaces",L"统一Unicode空白",true);check(L"fixSpaces",L"合并空格并去除首尾空格",true);check(L"emoji",L"移除表情符号");check(L"marks",L"移除组合音调标记");check(L"camel",L"拆分驼峰名称");check(L"sharepoint",L"清理SharePoint不支持的字符");}
 else if(type==L"Translit"){add(L"alphabetMap",L"内置Unicode转写方案",L"Any-Latin; Latin-ASCII",4,{L"Any-Latin; Latin-ASCII · 中文转拼音",L"Han-Latin · 中文转带声调拼音",L"Traditional-Simplified · 繁体转简体",L"Simplified-Traditional · 简体转繁体",L"Cyrillic-Latin · 西里尔字母转拉丁字母",L"Greek-Latin · 希腊字母转拉丁字母",L"Latin-ASCII · 去掉字母音调",L"Any-Upper · 转为大写",L"Any-Lower · 转为小写"});multi(L"map",L"自定义映射（每行 原文=替换文，填写后优先使用）");check(L"reverse",L"反向转写 / 交换映射");check(L"case",L"自定义映射区分大小写");}
 else if(type==L"ReformatDate"){edit(L"inputFormat",L"输入日期格式（多项以 | 分隔）",L"yyyy-mm-dd|yyyymmdd");edit(L"outputFormat",L"输出日期格式",L"yyyy-mm-dd");edit(L"adjust",L"时间调整：年,月,日,时,分,秒,毫秒",L"0,0,0,0,0,0,0");edit(L"shortMonths",L"自定义12个月缩写（逗号分隔，可留空）");edit(L"longMonths",L"自定义12个月全名（逗号分隔，可留空）");check(L"whole",L"仅匹配完整日期单词");}
 else if(type==L"PascalScript"){multi(L"script",L"重命名脚本",L"begin\r\n  FileName := WideUpperCase(FileName);\r\nend.");check(L"allowIO",L"使用文件和外部程序");}
 else if(type==L"UserInput"||type==L"Mapping"){combo(L"inputSource",L"数据来源",L"text",{L"text · 下方文本",L"file · 文本文件",L"clipboard · 剪贴板"});edit(L"inputFile",L"文本文件完整路径");combo(L"encoding",L"文件编码",L"utf8",{L"utf8 · UTF-8",L"utf16 · UTF-16 LE",L"gb18030 · GB18030"});if(type==L"UserInput")multi(L"list",L"名称列表（每行对应一个项目）");else{multi(L"mapping",L"原名、新名两列的CSV");combo(L"delimiter",L"列分隔符",L"comma",{L"comma · 逗号",L"semicolon · 分号",L"tab · 制表符"});check(L"header",L"第一行是表头");check(L"fullPath",L"原名列匹配完整路径");check(L"original",L"匹配原名称（否则匹配前一规则的结果）");check(L"case",L"区分大小写");}}
 else if(type==L"Metadata"){
  if(!o.count(L"metaTag")||o[L"metaTag"].empty())o[L"metaTag"]=L"File_Name";
  std::vector<std::wstring>categories={L"全部"};for(const auto&row:batchMetaRows())if(row.size()==3&&std::find(categories.begin(),categories.end(),row[0])==categories.end())categories.push_back(row[0]);combo(L"uiCategory",L"信息类别",L"全部",categories);
  std::vector<std::wstring>tags,optional={L" · 不添加"};auto category=o.count(L"uiCategory")?o[L"uiCategory"]:L"全部";
  for(const auto&row:batchMetaRows())if(row.size()==3&&(category==L"全部"||row[0]==category||row[1]==o[L"metaTag"]||row[1]==o[L"metaTag2"])){auto entry=row[1]+L" · "+batchMetaLabel(row);tags.push_back(entry);optional.push_back(entry);}
  combo(L"metaTag",L"使用哪项信息",L"File_Name",tags);edit(L"prefix",L"前面加字");combo(L"metaTag2",L"再加一项信息",L"",optional);edit(L"separator",L"两项之间加字",L"_");edit(L"suffix",L"后面加字");
  combo(L"dateFormat",L"日期显示方式",L"yyyy-MM-dd",{L"yyyy-MM-dd · 2026-10-29",L"yyyyMMdd · 20261029",L"yyyy年MM月dd日 · 2026年10月29日",L"yyyy-MM-dd HHnnss · 日期和时间"});
  f.fields.push_back({580,L"第一个文件的示例",L"",0,{},true,L"",true});
 }
 if(!metadataTargets.empty())combo(L"dateFormat",L"日期显示方式",L"yyyy-MM-dd",{L"yyyy-MM-dd · 2026-10-29",L"yyyyMMdd · 20261029",L"yyyy-MM-dd HHnnss · 日期和时间"});
 int skipId=check(L"skip",L"不改扩展名",step.skipExtension);(void)skipId;
 bool hasMore=false;for(const auto&entry:keys){auto k=entry.second;if(k==L"repeat"||k==L"resetEvery"||k==L"resetFolder"||k==L"resetName"||k==L"forceCase"||k==L"forceWhole"||k==L"extensionCase"||k==L"ranges"||k==L"seed"||k==L"allowIO"||k==L"adjust"||k==L"shortMonths"||k==L"longMonths"||k==L"metaTag2"||k==L"separator"||k==L"dateFormat"||k==L"sameReplacement"||k==L"multiline"||k==L"singleline")hasMore=true;}
 if(hasMore)f.fields.push_back({600,L"更多设置",drafts[type][L"uiMore"]==L"1"?L"1":L"0",1});
 return f.fields;
 };
 auto visibility=[&](EForm&p){
  auto get=[&](const wchar_t*k){for(const auto&entry:keys)if(entry.second==k)return selected(p,entry.first);return std::wstring();};
  bool more=p.check(600);for(auto&field:p.fields){if(field.id==100||field.id==600||field.id==580)continue;auto k=keys[field.id];bool optional=k==L"repeat"||k==L"resetEvery"||k==L"resetFolder"||k==L"resetName"||k==L"forceCase"||k==L"forceWhole"||k==L"extensionCase"||k==L"ranges"||k==L"seed"||k==L"allowIO"||k==L"adjust"||k==L"shortMonths"||k==L"longMonths"||k==L"metaTag2"||k==L"separator"||k==L"dateFormat"||k==L"sameReplacement"||k==L"multiline"||k==L"singleline";field.visible=!optional||more;
   if(k==L"position")field.visible=type==L"Delete"?get(L"from")==L"position":get(L"where")==L"position";
   if(k==L"anchor")field.visible=get(L"where")==L"before"||get(L"where")==L"after";
   if(k==L"rtl")field.visible=type==L"Rearrange"||type==L"Delete"||get(L"where")==L"position";
   if(type==L"Delete"){bool clear=false;for(const auto&e:keys)if(e.second==L"clear")clear=p.check(e.first);if(k!=L"clear"&&k!=L"skip")field.visible=!clear;if(k==L"fromText")field.visible=!clear&&get(L"from")==L"delimiter";if(k==L"untilText")field.visible=!clear&&get(L"until")==L"delimiter";if(k==L"count")field.visible=!clear&&get(L"until")==L"count";if(k==L"position")field.visible=!clear&&get(L"from")==L"position";}
   if(k==L"alphabet")field.visible=type==L"Serialize"?(get(L"system")==L"alphabetic"||get(L"system")==L"numeric"):get(L"randomChars")==L"custom";
   if(k==L"inputFile"||k==L"encoding")field.visible=get(L"inputSource")==L"file";
   if(k==L"list"||k==L"mapping")field.visible=get(L"inputSource")==L"text";
   if(type==L"Metadata"&&k==L"dateFormat")field.visible=more||get(L"metaTag").find(L"Date")!=std::wstring::npos;
   if(type==L"Metadata"&&k==L"separator")field.visible=more&&!get(L"metaTag2").empty();
  }
  bool extra=!metadataTargets.empty()||type==L"PascalScript"||type==L"Mapping"||type==L"UserInput";ShowWindow(p.ctl(990),extra?SW_SHOW:SW_HIDE);SetWindowTextW(p.ctl(990),type==L"PascalScript"?L"检查脚本":type==L"Mapping"?L"使用当前预览":type==L"UserInput"?L"使用当前预览":L"插入文件信息");
 };
 form.fields=build();
 form.changed=[&](EForm&p){if(p.changedId==100){save(p);drafts[type][L"uiMore"]=p.check(600)?L"1":L"0";auto label=p.get(100);for(const auto&entry:batch::ruleTypes())if(entry.second==label)type=entry.first;p.replaceFields(build());SetFocus(p.ctl(100));}
  else if(type==L"Metadata"&&keys[p.changedId]==L"uiCategory"){save(p);p.replaceFields(build());}
  visibility(p);if(type==L"Metadata"&&(p.changedId==0||p.changedId==100||keys[p.changedId]==L"metaTag"||keys[p.changedId]==L"uiCategory")){int id=0;for(const auto&e:keys)if(e.second==L"metaTag")id=e.first;p.set(580,batchMetaExample(selected(p,id)));}
 };
 form.extraLabel=L"插入文件信息";form.extra=[&](EForm&p){auto idFor=[&](const wchar_t*k){for(const auto&e:keys)if(e.second==k)return e.first;return 0;};
  if(!metadataTargets.empty()){int target=std::find(metadataTargets.begin(),metadataTargets.end(),p.focusField)!=metadataTargets.end()?p.focusField:metadataTargets[0];auto token=batchMetaPicker(p.window);if(!token.empty()){auto label=batchTemplateText(token,false);SendMessageW(p.ctl(target),EM_REPLACESEL,TRUE,(LPARAM)label.c_str());SetFocus(p.ctl(target));}return;}
  if(type==L"PascalScript"){std::wstring answer,error;bool ok=ever::advancedTransform(batch::jsonOptions({{L"type",L"PascalScript"},{L"script",p.get(idFor(L"script"))},{L"compileOnly",L"1"}}),L"[]",answer,error);MessageBoxW(p.window,ok?L"脚本没有问题。":error.c_str(),L"检查脚本",MB_OK);}
  else if(type==L"UserInput")p.set(idFor(L"list"),text());
  else if(type==L"Mapping"){auto names=eNameLines(text());std::wstring csv;auto quote=[](std::wstring s){size_t at=0;while((at=s.find(L'"',at))!=s.npos){s.insert(at,1,L'"');at+=2;}return L"\""+s+L"\"";};for(size_t i=0;i<items.size()&&i<names.size();i++){if(i)csv+=L"\r\n";csv+=quote(items[i].name)+L","+quote(names[i]+(hidden&&i<extensions.size()?extensions[i]:L""));}p.set(idFor(L"mapping"),csv);}
 };
 if(!form.run(window,dpi,L"重命名规则"))return false;save(form);auto options=drafts[type];options[L"uiMore"]=form.value(600)==L"1"?L"1":L"0";step.kind=batch::Kind::Advanced;step.skipExtension=options[L"skip"]==L"1";step.options=std::move(options);return true;
}
