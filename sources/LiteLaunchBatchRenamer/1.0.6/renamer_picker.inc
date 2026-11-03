// Members of EverWindow. Stable metadata identifiers stay inside presets and the engine.
static const std::vector<std::vector<std::wstring>>&batchMetaRows(){
 static std::vector<std::vector<std::wstring>>rows;if(rows.empty()){std::wstring result,error;if(ever::advancedTransform(L"{\"type\":\"Catalog\"}",L"[]",result,error))rows=batch::JsonRows(result).parse();}
 return rows;
}
static std::wstring batchMetaName(const std::vector<std::wstring>&row){if(row[1]==L"EXIF_DateTaken")return L"拍摄日期";return row[2];}
static std::wstring batchMetaLabel(const std::vector<std::wstring>&row){return row[0]+L"："+batchMetaName(row);}
static std::wstring batchTemplateText(std::wstring text,bool encode){
 auto replace=[&](const std::wstring&from,const std::wstring&to){size_t at=0;while((at=text.find(from,at))!=text.npos){text.replace(at,from.size(),to);at+=to.size();}};
 for(const auto&row:batchMetaRows()){if(row.size()!=3)continue;std::wstring token=L":"+row[1]+L":",label=L"["+batchMetaLabel(row)+L"]";replace(encode?label:token,encode?token:label);}
 for(const auto&pair:std::vector<std::pair<std::wstring,std::wstring>>{{L"{name}",L"[当前名称]"},{L"{num}",L"[编号]"},{L"{parent}",L"[父文件夹]"},{L"{taken}",L"[拍摄时间]"},{L"{created}",L"[创建时间]"},{L"{modified}",L"[修改时间]"},{L"{now}",L"[当前时间]"}})replace(encode?pair.second:pair.first,encode?pair.first:pair.second);
 return text;
}
std::wstring batchMetaExample(const std::wstring&tag){
 if(items.empty())return L"添加文件后显示示例";
 std::map<std::wstring,std::wstring>o={{L"type",L"Metadata"},{L"pattern",L":"+tag+L":"},{L"skip",L"0"},{L"missing",L"fallback"},{L"fallback",L"没有这项信息"},{L"sanitize",L"0"}};
 std::wstring answer,error;const auto&item=items[0];if(!ever::advancedTransform(batch::jsonOptions(o),batch::jsonRows({{item.path,item.name,item.name,item.folder?L"1":L"0",L""}}),answer,error))return L"没有这项信息";auto result=batch::JsonRows(answer).parse();return result.empty()?L"没有这项信息":result[0][0];
}
std::wstring batchMetaPicker(HWND owner){
 const auto&rows=batchMetaRows();std::vector<std::wstring>categories={L"全部"};for(const auto&row:rows)if(row.size()==3&&std::find(categories.begin(),categories.end(),row[0])==categories.end())categories.push_back(row[0]);
 EForm f;f.textChanged=true;f.fields={{101,L"类别",L"全部",4,categories},{102,L"搜索",L""},{103,L"文件信息",L"",4,{}},{104,L"第一个文件的示例",L"",2}};f.fields.back().readOnly=true;
 std::vector<std::vector<std::wstring>>matches;
 f.changed=[&](EForm&p){if(p.changedId==0||p.changedId==101||p.changedId==102){matches.clear();std::vector<std::wstring>labels;auto filter=p.get(102);std::transform(filter.begin(),filter.end(),filter.begin(),towlower);for(const auto&row:rows){if(row.size()!=3||p.get(101)!=L"全部"&&p.get(101)!=row[0])continue;std::wstring label=batchMetaLabel(row),search=label;std::transform(search.begin(),search.end(),search.begin(),towlower);if(!filter.empty()&&search.find(filter)==search.npos)continue;matches.push_back(row);labels.push_back(label);}p.choices(103,labels,labels.empty()?L"":labels[0]);}
  if(p.changedId!=104){int index=(int)SendMessageW(p.ctl(103),CB_GETCURSEL,0,0);bool before=p.notifying;p.notifying=true;p.set(104,index>=0&&index<(int)matches.size()?batchMetaExample(matches[index][1]):L"没有找到匹配项");p.notifying=before;EnableWindow(p.ctl(IDOK),index>=0);}
 };
 if(!f.run(owner,dpi,L"选择文件信息"))return L"";for(const auto&row:matches)if(batchMetaLabel(row)==f.value(103))return L":"+row[1]+L":";return L"";
}
static std::wstring batchRuleSummary(const batch::Step&step){
 auto get=[&](const wchar_t*key){auto it=step.options.find(key);return it==step.options.end()?std::wstring():it->second;};
 auto friendly=[&](std::wstring value){value=batchTemplateText(value,false);size_t at=0;while((at=value.find(L'[',at))!=value.npos){size_t end=value.find(L']',at+1);if(end==value.npos)break;auto label=value.substr(at+1,end-at-1);bool known=false;for(const auto&row:batchMetaRows())if(batchMetaLabel(row)==label){label=batchMetaName(row);known=true;break;}for(const auto&alias:std::vector<std::wstring>{L"当前名称",L"编号",L"父文件夹",L"拍摄时间",L"创建时间",L"修改时间",L"当前时间"})if(label==alias)known=true;if(!known){at=end+1;continue;}auto chip=L"【"+label+L"】";value.replace(at,end-at+1,chip);at+=chip.size();}std::replace(value.begin(),value.end(),L'\n',L' ');return value;};
 if(step.kind!=batch::Kind::Advanced)return friendly(batch::summary(step));
 auto type=get(L"type"),label=batch::ruleLabel(type);std::wstring detail;
 if(type==L"Insert")detail=friendly(get(L"text"));
 else if(type==L"Metadata")detail=friendly(get(L"pattern"));
 else if(type==L"Replace"||type==L"RegEx")detail=get(L"find")+L" → "+friendly(get(L"replace"));
 else if(type==L"Remove")detail=get(L"find");
 else if(type==L"Extension")detail=get(L"detect")==L"1"?L"自动识别":get(L"extension");
 else if(type==L"Serialize")return batch::summary(step);
 return label+(detail.empty()?L"":L"："+detail);
}
