#pragma once
#include <windows.h>
#include <string>

/* Independently implemented rules matching the documented EVER workflow.
   Names are the current editable text (the caller decides whether extension is hidden).
   This engine never renames, writes, scans directories, or launches Quicker. */
namespace ever {
enum class Mode { AddDeleteConvert, FindReplace, ExtractBeginning, PartialReplace, NumberArrange, NamingTemplate };
enum class Operation { Add, Delete, Convert };
enum class Position { Prefix, Suffix, Index, BeforeText, AfterText };
enum class AddSource { Text, Sequence, CreatedTime, ModifiedTime, PhotoTime, ParentName, CurrentTime, PinyinInitials, ChineseNumbers };
enum class Conversion { Upper, Lower, PinyinInitials, ChineseNumbers, Simplified, Traditional, RemoveWhitespace };
struct Rule {
    Mode mode = Mode::AddDeleteConvert;
    Operation operation = Operation::Add;
    Position position = Position::Prefix;
    AddSource source = AddSource::Text;
    Conversion conversion = Conversion::Upper;
    std::wstring text, anchor, separator = L"_";
    // Original position 0/1 is prefix; -1 is suffix; other positions are one-based.
    int index = 0, count = 1;
    std::wstring sequenceStart = L"01";
    long long sequenceStep = 1;
    int sequenceWidth = 0; // 0 uses original count+start padding branches; -1 retains input width.
    std::wstring timeFormat = L"yyyy-MM-dd HH-mm-ss";
    // Extension is protected by the caller. Tokens describe the name stem only.
    std::wstring namingPattern = L"{name}-{num}";
    bool replaceEntireName = false;
    bool replaceOriginalNumbers = false;
    int pinyinCount = 0;
    std::wstring frontAnchor, backAnchor;
    // Each nonempty line: find|replacement (or ||| / first-line |=delimiter). Applied in order.
    std::wstring replacements;
    bool regex = true, ignoreCase = true, singleline = false, multiline = true, unescape = false;
    bool escapeFind = false, escapeReplacement = true; // Decode \r/\n/\t, following Quicker's labels.
    // Legacy range fields retained in serialized templates. Original PartialReplace acts on
    // originalName as one joined multiline text; these range fields do not restrict that mode.
    int rangeStart = 1, rangeLength = 1;
    int extractLength = 12;
};
struct Input {
    std::wstring path, currentName, originalName;
    size_t ordinal = 0; // Zero-based current row order for sequence rules.
    size_t totalRows = 1; // Original EVER's automatic number padding is based on batch size.
};
bool apply(const Rule &rule,const Input &input,std::wstring &output,std::wstring &error);
std::wstring serialize(const Rule &rule);
bool deserialize(const std::wstring &text,Rule &rule,std::wstring &error);
}
