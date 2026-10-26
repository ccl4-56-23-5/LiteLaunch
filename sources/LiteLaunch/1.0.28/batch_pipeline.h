#pragma once
#include "ever_rules.h"
#include <vector>
#include <algorithm>
#include <cwctype>
#include <map>
namespace batch {
enum class Kind { Rule, Extension, Strip };
struct Step {Kind kind=Kind::Rule;bool enabled=true,skipExtension=true;ever::Rule rule;std::wstring text;};
struct Item {std::wstring path,name;bool folder=false;};
inline std::wstring extension(const std::wstring &name,bool folder){size_t dot=name.find_last_of(L'.');return !folder&&dot!=name.npos&&dot>0?name.substr(dot):L"";}
inline bool validUtf16(const std::wstring &text){for(size_t i=0;i<text.size();i++){unsigned c=text[i];if(c>=0xd800&&c<=0xdbff){if(++i>=text.size()||text[i]<0xdc00||text[i]>0xdfff)return false;}else if(c>=0xdc00&&c<=0xdfff)return false;}return true;}
inline bool preview(const std::vector<Step>&steps,const std::vector<Item>&items,std::vector<std::wstring>&output,std::wstring&error){
 error.clear();std::vector<std::wstring> names;for(const auto&i:items)names.push_back(i.name);
 if(steps.size()>128){error=L"最多添加128条规则。";return false;}
 for(size_t s=0;s<steps.size();s++){
  const Step &step=steps[s];if(!step.enabled)continue;
  std::vector<std::wstring> next;next.reserve(names.size());
  for(size_t i=0;i<names.size();i++){
   const std::wstring &name=names[i];std::wstring result=name;
   if(step.kind==Kind::Extension){
    if(!items[i].folder){std::wstring ext=step.text;while(!ext.empty()&&ext[0]==L'.')ext.erase(ext.begin());if(ext.find_first_of(L"\\/:*?\"<>|\r\n")!=ext.npos){error=L"扩展名含非法字符。";return false;}std::wstring old=extension(name,false);result=name.substr(0,name.size()-old.size())+(ext.empty()?L"":L"."+ext);}
   }else{
    std::wstring ext=step.skipExtension?extension(name,items[i].folder):L"",stem=name.substr(0,name.size()-ext.size());
    if(step.kind==Kind::Strip){
     result.clear();for(size_t n=0;n<stem.size();n++){std::wstring character(1,stem[n]);if(stem[n]>=0xd800&&stem[n]<=0xdbff&&n+1<stem.size())character+=stem[++n];if(step.text.find(character)==step.text.npos)result+=character;}
    }else{
     ever::Input input;input.path=items[i].path;input.currentName=stem;input.originalName=stem;input.ordinal=i;input.totalRows=items.size();
     if(!ever::apply(step.rule,input,result,error)){error=L"规则"+std::to_wstring(s+1)+L" · "+items[i].name+L"："+error;return false;}
    }
    result+=ext;
   }
   if(result.empty()||result.size()>255||result.find_first_of(L"\r\n")!=result.npos||!validUtf16(result)){error=L"规则"+std::to_wstring(s+1)+L"生成了空名称、超长名称或无效字符；原预览保持不变。";return false;}
   next.push_back(result);
  }
  names.swap(next);
 }
 output.swap(names);return true;
}
inline std::wstring summary(const Step&s){
 if(s.kind==Kind::Extension)return L"扩展名 → "+s.text;if(s.kind==Kind::Strip)return L"移除字符："+s.text;
 const auto&r=s.rule;
 if(r.mode==ever::Mode::NamingTemplate)return L"编号 / 模板："+r.namingPattern;
 if(r.mode==ever::Mode::FindReplace){size_t start=r.replacements.compare(0,4,L"|=\u241e\n")==0?4:0;wchar_t divider=start?L'\u241e':L'|';size_t at=r.replacements.find(divider,start);std::wstring label=at==std::wstring::npos?r.replacements:r.replacements.substr(start,at-start)+L" → "+r.replacements.substr(at+1);std::replace(label.begin(),label.end(),L'\n',L' ');return (r.regex?L"正则替换：":L"文本替换：")+label;}
 if(r.mode==ever::Mode::ExtractBeginning)return L"取文首";
 if(r.mode==ever::Mode::NumberArrange)return L"重排编号";
 if(r.operation==ever::Operation::Convert)return r.conversion==ever::Conversion::Upper?L"转换大写":r.conversion==ever::Conversion::Lower?L"转换小写":r.conversion==ever::Conversion::RemoveWhitespace?L"清理空白":L"文本转换";
 if(r.operation==ever::Operation::Delete)return L"删除：位置"+std::to_wstring(r.index)+L"，数量"+std::to_wstring(r.count);
 return L"插入："+r.text;
}
}
