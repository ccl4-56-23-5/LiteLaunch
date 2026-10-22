#pragma once
#include <windows.h>
#include <string>
namespace ever {
/* Embedded worker resource: 301/RCDATA. One private .NET Framework process is reused
   until regexShutdown; all user text travels over length-prefixed UTF-8 pipes. */
bool regexReplace(const std::wstring &pattern,const std::wstring &replacement,const std::wstring &input,
    bool regex,bool ignoreCase,bool singleline,bool multiline,bool escapeFind,bool escapeReplacement,
    std::wstring &output,std::wstring &error);
void regexShutdown();
bool regexFind(const std::wstring&pattern,const std::wstring&input,int startUtf16,bool ignoreCase,bool singleline,bool multiline,
    int&outStartUtf16,int&outLengthUtf16,std::wstring&error);
bool dotnetFormatTime(const SYSTEMTIME&time,const std::wstring&format,std::wstring&output,std::wstring&error);
bool dotnetSequence(const std::wstring&start,size_t ordinal,size_t totalRows,long long step,int width,bool numberArrange,std::wstring&output,std::wstring&error);
#ifdef EVER_REGEX_TESTING
void regexTestExecutable(const std::wstring &path);
#endif
}
