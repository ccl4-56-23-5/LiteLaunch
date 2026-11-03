#pragma once
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
namespace batch {
inline const std::vector<std::pair<std::wstring,std::wstring>>&ruleTypes(){static const std::vector<std::pair<std::wstring,std::wstring>>types={{L"Insert",L"添加文字"},{L"Delete",L"删除 / 清空"},{L"Remove",L"删除指定文字"},{L"Replace",L"替换文字"},{L"Rearrange",L"分段重排"},{L"Extension",L"修改扩展名"},{L"Strip",L"移除字符"},{L"Case",L"大小写"},{L"Serialize",L"编号"},{L"CleanUp",L"清理名称"},{L"Translit",L"文字转换"},{L"ReformatDate",L"日期格式"},{L"PascalScript",L"脚本"},{L"RegEx",L"正则替换"},{L"UserInput",L"按名单命名"},{L"Randomize",L"随机编号"},{L"Mapping",L"名称对照表"},{L"Padding",L"补齐位数"},{L"Metadata",L"文件信息"}};return types;}
inline std::wstring ruleLabel(const std::wstring&type){for(const auto&entry:ruleTypes())if(entry.first==type)return entry.second;return L"重命名";}
inline std::wstring jsonQuote(const std::wstring&s){std::wstring out=L"\"";const wchar_t*hex=L"0123456789abcdef";for(wchar_t c:s){if(c==L'"'||c==L'\\'){out+=L'\\';out+=c;}else if(c<32){out+=L"\\u00";out+=hex[(c>>4)&15];out+=hex[c&15];}else out+=c;}return out+L'"';}
inline std::wstring jsonOptions(const std::map<std::wstring,std::wstring>&options){std::wstring out=L"{";for(const auto&kv:options){if(out.size()>1)out+=L',';out+=jsonQuote(kv.first)+L':'+jsonQuote(kv.second);}return out+L'}';}
inline std::wstring jsonRows(const std::vector<std::vector<std::wstring>>&rows){std::wstring out=L"[";for(const auto&r:rows){if(out.size()>1)out+=L',';out+=L'[';bool first=true;for(const auto&s:r){if(!first)out+=L',';first=false;out+=jsonQuote(s);}out+=L']';}return out+L']';}
class JsonRows {
 const std::wstring&s;size_t at=0;
 void ws(){while(at<s.size()&&iswspace(s[at]))at++;}
 bool take(wchar_t c){ws();if(at<s.size()&&s[at]==c){at++;return true;}return false;}
 void need(wchar_t c){if(!take(c))throw std::runtime_error("Invalid worker JSON");}
 std::wstring str(){need(L'"');std::wstring out;while(at<s.size()){wchar_t c=s[at++];if(c==L'"')return out;if(c==L'\\'){if(at==s.size())break;c=s[at++];if(c==L'u'){unsigned n=0;for(int i=0;i<4;i++){if(at==s.size())throw std::runtime_error("Invalid JSON escape");wchar_t h=s[at++];int v=h>=L'0'&&h<=L'9'?h-L'0':h>=L'a'&&h<=L'f'?h-L'a'+10:h>=L'A'&&h<=L'F'?h-L'A'+10:-1;if(v<0)throw std::runtime_error("Invalid JSON escape");n=n*16+v;}c=(wchar_t)n;}else if(c==L'n')c=L'\n';else if(c==L'r')c=L'\r';else if(c==L't')c=L'\t';else if(c==L'b')c=L'\b';else if(c==L'f')c=L'\f';else if(c!=L'"'&&c!=L'\\'&&c!=L'/')throw std::runtime_error("Invalid JSON escape");}out+=c;}throw std::runtime_error("Unclosed JSON string");}
public: explicit JsonRows(const std::wstring&text):s(text){}
 std::vector<std::vector<std::wstring>> parse(){std::vector<std::vector<std::wstring>> out;need(L'[');if(!take(L']')){do{need(L'[');std::vector<std::wstring> row;if(!take(L']')){do{row.push_back(str());}while(take(L','));need(L']');}out.push_back(row);if(out.size()>100000)throw std::runtime_error("Too many rows");}while(take(L','));need(L']');}ws();if(at!=s.size())throw std::runtime_error("Trailing JSON data");return out;}
};
}
