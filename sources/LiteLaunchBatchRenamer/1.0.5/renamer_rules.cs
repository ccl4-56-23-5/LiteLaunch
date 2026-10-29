using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Globalization;
using System.Collections;
using System.Collections.Generic;
using System.Numerics;
using System.Security.Cryptography;
using System.Web.Script.Serialization;

// Independently implemented documented renaming behaviours. This engine computes names;
// only the native transaction layer commits a rename.
internal static class RenamerRules {
    internal static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength=16*1024*1024, RecursionLimit=64 };
    internal static string Get(IDictionary<string,object> o,string k,string fallback="") { object v;return o.TryGetValue(k,out v)&&v!=null?Convert.ToString(v,CultureInfo.InvariantCulture):fallback; }
    internal static bool Flag(IDictionary<string,object> o,string k,bool fallback=false) {return Get(o,k,fallback?"1":"0")=="1";}
    internal static int Number(IDictionary<string,object> o,string k,int fallback,int min,int max) {int n;string s=Get(o,k,fallback.ToString(CultureInfo.InvariantCulture));if(!Int32.TryParse(s,NumberStyles.Integer,CultureInfo.InvariantCulture,out n)||n<min||n>max)throw new ArgumentException(k+" 数值超出范围："+min+"—"+max);return n;}
    internal static string[] Lines(string s){return s.Replace("\r\n","\n").Replace('\r','\n').Split('\n');}
    internal static List<string> Chars(string s){var a=new List<string>();for(int i=0;i<s.Length;i++){if(Char.IsHighSurrogate(s[i])&&i+1<s.Length&&Char.IsLowSurrogate(s[i+1]))a.Add(s.Substring(i++,2));else a.Add(s.Substring(i,1));}return a;}
    internal static string Slice(List<string>a,int from,int count){var b=new StringBuilder();for(int i=Math.Max(0,from);i<Math.Min(a.Count,from+count);i++)b.Append(a[i]);return b.ToString();}
    internal static Regex Rx(string pattern,RegexOptions flags=RegexOptions.None){return new Regex(pattern,flags,TimeSpan.FromSeconds(2));}
    internal static string Ext(string name,bool folder){int d=name.LastIndexOf('.');return !folder&&d>0?name.Substring(d):"";}
    internal static string Put(string name,string add,IDictionary<string,object>o){
        string where=Get(o,"where","prefix");if(where=="replace")return add;if(where=="prefix")return add+name;if(where=="suffix")return name+add;
        if(where=="before"||where=="after"){string anchor=Get(o,"anchor");if(anchor.Length==0)throw new ArgumentException("定位文字不能为空。");int p=name.IndexOf(anchor,Flag(o,"case")?StringComparison.Ordinal:StringComparison.OrdinalIgnoreCase);return p<0?name:name.Insert(p+(where=="after"?anchor.Length:0),add);}
        var chars=Chars(name);int n=Number(o,"position",1,1,1000000)-1;int at=Flag(o,"rtl")?Math.Max(0,chars.Count-n):Math.Min(chars.Count,n);return Slice(chars,0,at)+add+Slice(chars,at,chars.Count-at);
    }
    private static string Wildcard(string s){
        var b=new StringBuilder();for(int i=0;i<s.Length;i++){char c=s[i];if(c=='*')b.Append(".*");else if(c=='?')b.Append("(?:[\\uD800-\\uDBFF][\\uDC00-\\uDFFF]|[^\\uD800-\\uDFFF])");else if(c=='['){int e=s.IndexOf(']',i+1);if(e>i+1){string set=s.Substring(i+1,e-i-1);b.Append('[');if(set.StartsWith("!")){b.Append('^');set=set.Substring(1);}b.Append(set.Replace("\\","\\\\"));b.Append(']');i=e;}else b.Append("\\[");}else b.Append(Regex.Escape(c.ToString()));}return b.ToString();
    }
    private static string Replace(string s,string find,string replacement,IDictionary<string,object>o,bool regex){
        string pattern=regex?find:Flag(o,"wildcards")?Wildcard(find):Regex.Escape(find);if(Flag(o,"whole"))pattern="(?<![\\p{L}\\p{N}_])(?:"+pattern+")(?![\\p{L}\\p{N}_])";
        RegexOptions flags=Flag(o,"case")?RegexOptions.None:RegexOptions.IgnoreCase;if(Flag(o,"multiline"))flags|=RegexOptions.Multiline;if(Flag(o,"singleline"))flags|=RegexOptions.Singleline;
        var re=Rx(pattern,flags);string occurrence=Get(o,"occurrence","all");int selected=-1;if(occurrence=="last"){var matches=re.Matches(s);if(matches.Count>0)selected=matches[matches.Count-1].Index;}
        bool first=true;return re.Replace(s,delegate(Match m){if(occurrence=="last"&&m.Index!=selected)return m.Value;if(occurrence=="first"&&!first)return m.Value;first=false;return regex?m.Result(replacement):replacement;});
    }
    private static string Delete(string s,IDictionary<string,object>o){
        if(Flag(o,"clear"))return "";var chars=Chars(s);bool reverse=Flag(o,"rtl");if(reverse)chars.Reverse();string work=String.Concat(chars);int from;
        if(Get(o,"from","position")=="delimiter"){
            string a=Get(o,"fromText");if(a.Length==0)throw new ArgumentException("起始分隔符不能为空。");if(reverse){var ac=Chars(a);ac.Reverse();a=String.Concat(ac);}int p=work.IndexOf(a,StringComparison.Ordinal);if(p<0)return s;from=Chars(work.Substring(0,p)).Count;if(Flag(o,"keepDelimiters"))from+=Chars(a).Count;
        }else from=Math.Min(chars.Count,Number(o,"position",1,1,1000000)-1);
        int end=chars.Count;string until=Get(o,"until","count");
        if(until=="count")end=Math.Min(chars.Count,from+Number(o,"count",1,0,1000000));
        else if(until=="delimiter"){
            string a=Get(o,"untilText");if(a.Length==0)throw new ArgumentException("结束分隔符不能为空。");if(reverse){var ac=Chars(a);ac.Reverse();a=String.Concat(ac);}string remaining=Slice(chars,from,chars.Count-from);int p=remaining.IndexOf(a,StringComparison.Ordinal);if(p<0)return s;end=from+Chars(remaining.Substring(0,p)).Count+(Flag(o,"keepDelimiters")?0:Chars(a).Count);
        }
        if(end>from)chars.RemoveRange(from,end-from);if(reverse)chars.Reverse();return String.Concat(chars);
    }
    private static string Rearrange(string s,IDictionary<string,object>o,string path,string original){
        string mode=Get(o,"split","delimiters"), spec=Get(o,"splitBy"," - ");string[] specs=spec.Split('|');var parts=new List<string>();
        if(mode=="positions"){
            var chars=Chars(s);if(Flag(o,"rtl"))chars.Reverse();var cuts=new SortedSet<int>();cuts.Add(0);cuts.Add(chars.Count);foreach(string p in specs){int n;if(!Int32.TryParse(p,out n)||n<1)throw new ArgumentException("拆分位置必须为正整数。");cuts.Add(Math.Min(n-1,chars.Count));}int before=-1;foreach(int cut in cuts){if(before>=0){var sub=Chars(Slice(chars,before,cut-before));if(Flag(o,"rtl"))sub.Reverse();parts.Add(String.Concat(sub));}before=cut;}
        }else if(mode=="exact"){
            int start=0;foreach(string d in specs){if(d.Length==0)throw new ArgumentException("分隔符不能为空。");int at=s.IndexOf(d,start,StringComparison.Ordinal);if(at<0)return s;parts.Add(s.Substring(start,at-start));start=at+d.Length;}parts.Add(s.Substring(start));if(Flag(o,"rtl"))parts.Reverse();
        }else{
            foreach(string d in specs)if(d.Length==0)throw new ArgumentException("分隔符不能为空。");Array.Sort(specs,delegate(string a,string b){return b.Length.CompareTo(a.Length);});string pattern=String.Join("|",Array.ConvertAll(specs,Regex.Escape));parts.AddRange(Rx(pattern).Split(s));if(Flag(o,"rtl"))parts.Reverse();
        }
        string result=Rx(@"\$(-?\d+)|\$\$").Replace(Get(o,"pattern","$2 - $1"),delegate(Match m){if(m.Value=="$$")return "$";int n=Int32.Parse(m.Groups[1].Value);if(n==0)return s;int at=n>0?n-1:parts.Count+n;return at>=0&&at<parts.Count?parts[at]:"";});return RenamerMetadata.Expand(result,path,original,o);
    }
    private static string Radix(BigInteger value,List<string> alphabet){if(alphabet.Count<2)throw new ArgumentException("编号字母表至少需要两个不同字符。");bool minus=value<0;value=BigInteger.Abs(value);var b=new StringBuilder();do{BigInteger remainder;value=BigInteger.DivRem(value,alphabet.Count,out remainder);b.Insert(0,alphabet[(int)remainder]);}while(value>0);return (minus?"-":"")+b.ToString();}
    private static string Chinese(BigInteger value){
        if(value==0)return "零";if(value<0)return "负"+Chinese(-value);string[] units={"","万","亿","兆","京","垓","秭","穰"};var groups=new List<int>();while(value>0){BigInteger r;value=BigInteger.DivRem(value,10000,out r);groups.Add((int)r);}if(groups.Count>units.Length)throw new ArgumentException("中文编号过大。");var b=new StringBuilder();bool zero=false;for(int g=groups.Count-1;g>=0;g--){int n=groups[g];if(n==0){if(b.Length>0)zero=true;continue;}if(b.Length>0&&(zero||n<1000))b.Append('零');zero=false;string digits="零一二三四五六七八九";string[] small={"","十","百","千"};bool pending=false,groupStarted=false;for(int k=3;k>=0;k--){int power=(int)Math.Pow(10,k),d=n/power;n%=power;if(d!=0){if(pending)b.Append('零');b.Append(digits[d]);b.Append(small[k]);pending=false;groupStarted=true;}else if(groupStarted&&n>0)pending=true;}b.Append(units[g]);}return b.ToString();
    }
    internal static string Serial(BigInteger value,IDictionary<string,object>o){
        string system=Get(o,"system","decimal"),text;string zero="0";
        if(system=="roman"){if(value<1||value>399999)throw new ArgumentException("罗马编号支持1—399999。");int n=(int)value;var b=new StringBuilder();int[] values={1000,900,500,400,100,90,50,40,10,9,5,4,1};string[] symbols={"M","CM","D","CD","C","XC","L","XL","X","IX","V","IV","I"};for(int i=0;i<values.Length;i++)while(n>=values[i]){b.Append(symbols[i]);n-=values[i];}text=b.ToString();}
        else if(system=="music"){if(value<1)throw new ArgumentException("音符编号从1开始。");BigInteger oct,rem;oct=BigInteger.DivRem(value-1,12,out rem);text=new[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}[(int)rem]+oct.ToString(CultureInfo.InvariantCulture);}
        else if(system=="chinese")text=Chinese(value);
        else if(system=="letters"||system=="alphabetic"||system=="numeric"){
            var alphabet=Chars(system=="letters"?"abcdefghijklmnopqrstuvwxyz":Get(o,"alphabet","01"));if(system!="numeric"&&value<1)throw new ArgumentException("字母序列从1开始。");text=Radix(system=="numeric"?value:value-1,alphabet);zero=alphabet[0];
        }else text=value.ToString(CultureInfo.InvariantCulture);
        int width=Number(o,"width",0,0,255);bool negative=text.StartsWith("-");string unsigned=negative?text.Substring(1):text;while(Chars(unsigned).Count<width)unsigned=zero+unsigned;return (negative?"-":"")+unsigned;
    }
    private static string Strip(string s,IDictionary<string,object>o){
        string custom=Get(o,"characters");string sets=Get(o,"sets");if(sets.IndexOf("english",StringComparison.Ordinal)>=0)custom+="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";if(sets.IndexOf("digits",StringComparison.Ordinal)>=0)custom+="0123456789";if(sets.IndexOf("symbols",StringComparison.Ordinal)>=0)custom+="!\"#$%&'*+,-./:;=?@^_`|~";if(sets.IndexOf("brackets",StringComparison.Ordinal)>=0)custom+="(){}[]<>";
        var selected=new HashSet<string>(Chars(custom),Flag(o,"case")?StringComparer.Ordinal:StringComparer.OrdinalIgnoreCase);var ranges=new List<int[]>();foreach(string range in Get(o,"ranges").Split(',')){if(range.Trim().Length==0)continue;var pair=range.Trim().Split('-');int a,b;if(!Int32.TryParse(pair[0],NumberStyles.HexNumber,CultureInfo.InvariantCulture,out a)||a<0||a>0x10ffff)throw new ArgumentException("Unicode范围无效。");b=a;if(pair.Length>2||pair.Length==2&&(!Int32.TryParse(pair[1],NumberStyles.HexNumber,CultureInfo.InvariantCulture,out b)||b<a||b>0x10ffff))throw new ArgumentException("Unicode范围无效。");ranges.Add(new[]{a,b});}
        var chars=Chars(s);Func<string,bool> remove=delegate(string c){int cp=Char.ConvertToUtf32(c,0);bool match=selected.Contains(c);foreach(var r in ranges)if(cp>=r[0]&&cp<=r[1])match=true;return Flag(o,"invert")?!match:match;};string where=Get(o,"stripWhere","all");if(where=="leading"){while(chars.Count>0&&remove(chars[0]))chars.RemoveAt(0);}else if(where=="trailing"){while(chars.Count>0&&remove(chars[chars.Count-1]))chars.RemoveAt(chars.Count-1);}else chars.RemoveAll(delegate(string c){return remove(c);});return String.Concat(chars);
    }
    private static string ChangeCase(string s,IDictionary<string,object>o){
        string mode=Get(o,"caseMode","title");bool preserve=Flag(o,"preserve");CultureInfo culture=CultureInfo.CurrentCulture;
        if(mode=="upper")s=s.ToUpper(culture);else if(mode=="lower")s=s.ToLower(culture);
        else if(mode=="invert"){var b=new StringBuilder();foreach(string c in Chars(s))b.Append(c==c.ToUpper(culture)?c.ToLower(culture):c.ToUpper(culture));s=b.ToString();}
        else if(mode=="title"){if(!preserve)s=s.ToLower(culture);s=Rx(@"(?<![\p{L}\p{N}])\p{L}").Replace(s,delegate(Match m){return m.Value.ToUpper(culture);});}
        else if(mode=="first"||mode=="sentence"){if(!preserve)s=s.ToLower(culture);bool first=true;var b=new StringBuilder();foreach(string c in Chars(s)){if(first&&Char.IsLetter(c,0)){b.Append(c.ToUpper(culture));first=false;}else b.Append(c);if(mode=="sentence"&&(c=="."||c=="!"||c=="?"||c=="。"||c=="！"||c=="？"))first=true;}s=b.ToString();}
        foreach(string fragment in Get(o,"forceCase").Split(',')){if(fragment.Length==0)continue;string p=Regex.Escape(fragment);if(Flag(o,"forceWhole",true))p="(?<![\\p{L}\\p{N}])"+p+"(?![\\p{L}\\p{N}])";s=Rx(p,RegexOptions.IgnoreCase).Replace(s,delegate(Match m){return fragment;});}return s;
    }
    private static string Cleanup(string s,IDictionary<string,object>o){
        string brackets=Get(o,"brackets");string pattern="";if(brackets.Contains("()"))pattern=@"\([^()]*\)";if(brackets.Contains("[]"))pattern+=(pattern.Length>0?"|":"")+@"\[[^\[\]]*\]";if(brackets.Contains("{}"))pattern+=(pattern.Length>0?"|":"")+@"\{[^{}]*\}";if(pattern.Length>0){string next;do{next=Rx(pattern).Replace(s,"");if(next==s)break;s=next;}while(true);}
        string spaces=Get(o,"spaces","._");if(spaces.Length>0){var b=new StringBuilder();for(int i=0;i<s.Length;i++){bool keep=Flag(o,"keepNumbers",true)&&i>0&&i+1<s.Length&&Char.IsDigit(s[i-1])&&Char.IsDigit(s[i+1]);b.Append(spaces.IndexOf(s[i])>=0&&!keep?' ':s[i]);}s=b.ToString();}
        if(Flag(o,"unicodeSpaces",true))s=Rx(@"\s").Replace(s," ");if(Flag(o,"emoji")){var b=new StringBuilder();foreach(string c in Chars(s)){int n=Char.ConvertToUtf32(c,0);if(n>=0x1f000&&n<=0x1faff||n>=0x2600&&n<=0x27bf||n==0x200d||n==0xfe0f)continue;b.Append(c);}s=b.ToString();}
        if(Flag(o,"marks"))s=RenamerMetadata.WithoutMarks(s);
        if(Flag(o,"camel"))s=Rx(@"(?<!^)(?=\p{Lu})").Replace(s," ");
        if(Flag(o,"sharepoint")){s=s.Trim().Replace('{','(').Replace('}',')');s=Rx(@"\.{2,}").Replace(s,"");s=Rx("[\\x00-\\x1f/\\\\*?\"<>|:#%~&]").Replace(s,"");}
        if(Flag(o,"fixSpaces",true))s=Rx(" +").Replace(s," ").Trim();return s;
    }
    private static string Padding(string s,IDictionary<string,object>o){
        string mode=Get(o,"padMode","numbers");int width=Number(o,"width",3,0,255);if(mode=="remove")return Rx("[0-9]+").Replace(s,delegate(Match m){string v=m.Value.TrimStart('0');return v.Length==0?"0":v;});if(mode=="numbers")return Rx("[0-9]+").Replace(s,delegate(Match m){return m.Value.PadLeft(width,'0');});
        string chars=Get(o,"padText","0");if(chars.Length==0)throw new ArgumentException("填充字符不能为空。");var elements=Chars(s);var padding=Chars(chars);int n=Math.Max(0,width-elements.Count);var b=new StringBuilder();for(int i=0;i<n;i++)b.Append(padding[i%padding.Count]);return Get(o,"side","left")=="left"?b+s:s+b.ToString();
    }
    private static string ExternalText(IDictionary<string,object>o,string key){string source=Get(o,"inputSource","text");if(source=="clipboard")return RenamerMetadata.ClipboardText();if(source=="file"){string path=Get(o,"inputFile");if(!File.Exists(path))throw new FileNotFoundException("找不到输入列表文件。",path);var info=new FileInfo(path);if(info.Length>4*1024*1024)throw new ArgumentException("输入列表超过4MB。");string enc=Get(o,"encoding","utf8");Encoding encoding=enc=="gb18030"?Encoding.GetEncoding(54936):enc=="utf16"?Encoding.Unicode:new UTF8Encoding(false,true);return File.ReadAllText(path,encoding);}return Get(o,key);}
    private static List<string[]> Csv(string text,string delimiter){
        char d=delimiter=="tab"?'\t':delimiter=="semicolon"?';':',';var result=new List<string[]>();var row=new List<string>();var field=new StringBuilder();bool quoted=false;
        for(int i=0;i<=text.Length;i++){char c=i==text.Length?'\n':text[i];if(c=='"'){if(quoted&&i+1<text.Length&&text[i+1]=='"'){field.Append('"');i++;}else quoted=!quoted;}else if(!quoted&&(c==d||c=='\n'||c=='\r')){row.Add(field.ToString());field.Clear();if(c!=d){if(row.Count>1||row[0].Length>0)result.Add(row.ToArray());row.Clear();if(c=='\r'&&i+1<text.Length&&text[i+1]=='\n')i++;}}else field.Append(c);}
        if(quoted)throw new ArgumentException("CSV引号没有闭合。");return result;
    }
    internal static string Process(string settings,string inputs){
        var o=Json.Deserialize<Dictionary<string,object>>(settings);string type=Get(o,"type","Insert");
        if(type=="Catalog")return Json.Serialize(RenamerMetadata.Catalog);
        var raw=Json.Deserialize<object[]>(inputs);var rows=new List<string[]>();foreach(object value in raw){var arr=(object[])value;rows.Add(Array.ConvertAll(arr,delegate(object v){return Convert.ToString(v,CultureInfo.InvariantCulture);}));}
        if(rows.Count>100000)throw new ArgumentException("项目过多。");bool skip=Flag(o,"skip",true);var results=new List<string[]>();var subjects=new List<string>();foreach(var row in rows){string e=skip?row[4]:"";subjects.Add(e.Length>0&&row[1].EndsWith(e,StringComparison.Ordinal)?row[1].Substring(0,row[1].Length-e.Length):row[1]);}
        RenamerMetadata.Now=DateTime.Now;RenamerMetadata.CurrentCount=rows.Count;
        if(type=="PascalScript"){
            string[] computed=RenamerPascal.Run(Get(o,"script","begin FileName := 'Example ' + FileName; end."),rows,subjects,Flag(o,"allowIO"),Flag(o,"compileOnly"));
            for(int i=0;i<rows.Count;i++){string ext=skip?rows[i][4]:"";string n=computed[i]+ext;results.Add(new[]{n,skip?ext:Ext(n,rows[i][3]=="1"),computed[i].Length==0?"1":"0"});}return Json.Serialize(results);
        }
        BigInteger start=BigInteger.Zero,step=BigInteger.One;if(type=="Serialize"&&(!BigInteger.TryParse(Get(o,"start","1"),NumberStyles.Integer,CultureInfo.InvariantCulture,out start)||!BigInteger.TryParse(Get(o,"step","1"),NumberStyles.Integer,CultureInfo.InvariantCulture,out step)))throw new ArgumentException("编号起始和步长必须为整数。");
        int ordinal=0,repeat=type=="Serialize"?Number(o,"repeat",1,1,1000000):1,reset=type=="Serialize"?Number(o,"resetEvery",0,0,1000000):0;
        int seed;string seedText=Get(o,"seed","1");if(!Int32.TryParse(seedText,out seed))throw new ArgumentException("随机种子必须为整数。");var random=new Random(seed);var unique=new HashSet<string>(StringComparer.Ordinal);
        string[] list=type=="UserInput"?Lines(ExternalText(o,"list")):new string[0];List<string[]> mapping=type=="Mapping"?Csv(ExternalText(o,"mapping"),Get(o,"delimiter","comma")):new List<string[]>();
        RenamerMetadata.Now=DateTime.Now;RenamerMetadata.CurrentCount=rows.Count;
        for(int i=0;i<rows.Count;i++){
            var row=rows[i];bool folder=row[3]=="1";string path=row[0],original=row[2],ext=skip?row[4]:"",s=subjects[i],result=s;
            RenamerMetadata.CurrentIndex=i;RenamerMetadata.CurrentName=s;
            try{
            if(type=="Insert")result=Put(s,RenamerMetadata.Expand(Get(o,"text"),path,original,o),o);
            else if(type=="Delete")result=Delete(s,o);
            else if(type=="Remove"||type=="Replace"||type=="RegEx"){
                string[] find=Get(o,"find").Split(new[]{"*|*"},StringSplitOptions.None);string[] rep=Get(o,"replace").Split(new[]{"*|*"},StringSplitOptions.None);if(type=="RegEx")find=new[]{Get(o,"find")};
                for(int f=0;f<find.Length;f++){if(find[f].Length==0)throw new ArgumentException("查找内容不能为空。");string r=type=="Remove"?"":f<rep.Length?rep[f]:Flag(o,"sameReplacement")?rep[0]:"";r=RenamerMetadata.Expand(r,path,original,o);result=Replace(result,find[f],r,o,type=="RegEx");}
            }
            else if(type=="Rearrange")result=Rearrange(s,o,path,original);
            else if(type=="Extension"){
                if(folder){results.Add(new[]{row[1],"","0"});continue;}
                string previous=row[4];string stem=previous.Length>0&&row[1].EndsWith(previous,StringComparison.Ordinal)?row[1].Substring(0,row[1].Length-previous.Length):row[1];
                string newExt=Flag(o,"detect")?RenamerMetadata.Value(path,"File_ExtensionDetected",o):RenamerMetadata.Expand(Get(o,"extension"),path,original,o);newExt=newExt.TrimStart('.');if(newExt.IndexOfAny("\\/:*?\"<>|\r\n".ToCharArray())>=0)throw new ArgumentException("扩展名含非法字符。");ext=newExt.Length>0?"."+newExt:"";result=stem;
            }
            else if(type=="Strip")result=Strip(s,o);
            else if(type=="Case")result=ChangeCase(s,o);
            else if(type=="Serialize"){
                if(i>0&&(Flag(o,"resetFolder")&&!String.Equals(Path.GetDirectoryName(path),Path.GetDirectoryName(rows[i-1][0]),StringComparison.OrdinalIgnoreCase)||Flag(o,"resetName")&&s!=subjects[i-1]))ordinal=0;if(reset>0&&ordinal>=reset)ordinal=0;
                string number=Serial(start+(ordinal/repeat)*step,o);ordinal++;string add=RenamerMetadata.Expand(Get(o,"prefix"),path,original,o)+number+RenamerMetadata.Expand(Get(o,"suffix"),path,original,o);result=Put(s,add,o);
            }
            else if(type=="Randomize"){
                int length=Number(o,"length",8,1,255);string alphabetText=Get(o,"randomChars","digits");var alphabet=Chars(alphabetText=="digits"?"0123456789":alphabetText=="letters"?"abcdefghijklmnopqrstuvwxyz":Get(o,"alphabet","0123456789abcdef"));if(alphabet.Count<1)throw new ArgumentException("随机字符集不能为空。");var distinct=new List<string>(new HashSet<string>(alphabet));alphabet=distinct;BigInteger capacity=BigInteger.Pow(alphabet.Count,length);string candidate="";
                for(int attempt=0;attempt<4096;attempt++){var b=new StringBuilder();for(int p=0;p<length;p++)b.Append(alphabet[random.Next(alphabet.Count)]);candidate=b.ToString();if(!Flag(o,"unique",true)||!unique.Contains(candidate)||unique.Count>=capacity)break;if(attempt==4095){for(BigInteger n=0;n<capacity;n++){string v=Radix(n,alphabet);while(Chars(v).Count<length)v=alphabet[0]+v;if(!unique.Contains(v)){candidate=v;break;}}}}unique.Add(candidate);result=Put(s,candidate,o);
            }
            else if(type=="Padding")result=Padding(s,o);
            else if(type=="CleanUp")result=Cleanup(s,o);
            else if(type=="Translit")result=RenamerMetadata.Transliterate(s,Get(o,"alphabetMap","Any-Latin; Latin-ASCII"),Get(o,"map"),Flag(o,"reverse"),Flag(o,"case"));
            else if(type=="ReformatDate")result=RenamerMetadata.ReformatDates(s,o);
            else if(type=="UserInput"){if(i<list.Length)result=list[i];}
            else if(type=="Mapping"){
                int first=Flag(o,"header")?1:0;string lookup=Flag(o,"fullPath")?path:Flag(o,"original")?original:s;for(int m=first;m<mapping.Count;m++){var pair=mapping[m];if(pair.Length<2)throw new ArgumentException("映射CSV每行须有原名、新名两列。");if(String.Equals(lookup,pair[0],Flag(o,"case")?StringComparison.Ordinal:StringComparison.OrdinalIgnoreCase)){result=pair[1];break;}}
            }
            else if(type=="Metadata")result=RenamerMetadata.Expand(Get(o,"pattern",":File_Name:"),path,original,o);
            else throw new ArgumentException("未知规则："+type);
            if(type=="Case"&&!folder){string force=Get(o,"extensionCase","none");if(force!="none"){if(skip)ext=force=="upper"?ext.ToUpperInvariant():ext.ToLowerInvariant();else{string e=Ext(result,false);result=result.Substring(0,result.Length-e.Length)+(force=="upper"?e.ToUpperInvariant():e.ToLowerInvariant());}}}
            string name=result+ext;if(name.Length>255||name.IndexOfAny(new[]{'\r','\n','\0'})>=0)throw new ArgumentException("生成的名称过长或包含换行/空字符。");
            results.Add(new[]{name,type=="Extension"||skip?ext:Ext(name,folder),result.Length==0?"1":"0"});
            }catch(Exception e){throw new ArgumentException(original+"："+e.Message,e);}
        }
        return Json.Serialize(results);
    }
}
