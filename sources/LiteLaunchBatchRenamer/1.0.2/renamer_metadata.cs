using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Globalization;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.IO.Compression;
using System.Xml;
using System.Threading.Tasks;
using System.Numerics;

internal static class RenamerMetadata {
    // Categories and canonical tags are also consumed by the native searchable picker.
    internal static readonly string[][] Catalog=MakeCatalog();
    private static string[][] MakeCatalog(){
        var rows=new List<string[]>();string data=@"
文件|File_Name|文件主名称
文件|File_FileName|文件名（含扩展名）
文件|File_Extension|扩展名（不含点）
文件|File_ExtensionDetected|按内容检测扩展名
文件|File_FolderName|父文件夹名称
文件|File_FolderPath|父目录路径
文件|File_Path|完整路径
文件|File_Size|文件大小（字节）
文件|File_SizeKB|文件大小（KB）
文件|File_SizeMB|文件大小（MB）
文件|File_DateCreated|创建时间
文件|File_DateModified|修改时间
文件|File_DateAccessed|访问时间
文件|File_Attributes|文件属性
文件|File_Index|列表序号
文件|File_Count|列表项目数
文件|Current_Name|当前规则处理到的名称
校验值|File_CRC32|CRC32
校验值|File_MD5|MD5
校验值|File_SHA1|SHA-1
校验值|File_SHA256|SHA-256
校验值|File_SHA512|SHA-512
图片|Image_Width|图片宽度
图片|Image_Height|图片高度
图片|Image_AspectRatio|宽高比
图片|Image_MegaPixels|百万像素
图片|Image_BitDepth|位深
图片|Image_ColorSpace|色彩空间
图片|EXIF_DateTaken|拍摄时间
图片|EXIF_DateTimeOriginal|原始拍摄时间
图片|EXIF_DateTimeDigitized|数字化时间
图片|EXIF_Make|相机品牌
图片|EXIF_Model|相机型号
图片|EXIF_LensModel|镜头型号
图片|EXIF_LensMake|镜头品牌
图片|EXIF_SerialNumber|相机序列号
图片|EXIF_FNumber|光圈
图片|EXIF_ExposureTime|曝光时间
图片|EXIF_ISO|ISO
图片|EXIF_FocalLength|焦距
图片|EXIF_FocalLength35mm|等效焦距
图片|EXIF_Orientation|方向
图片|EXIF_Artist|作者
图片|EXIF_Copyright|版权
图片|EXIF_Description|描述
图片|EXIF_Software|软件
图片|EXIF_ExposureProgram|曝光程序
图片|EXIF_ExposureBias|曝光补偿
图片|EXIF_MeteringMode|测光模式
图片|EXIF_Flash|闪光灯
图片|EXIF_WhiteBalance|白平衡
图片|EXIF_GPSLatitude|纬度
图片|EXIF_GPSLongitude|经度
图片|EXIF_GPSAltitude|海拔
图片|IPTC_ObjectName|IPTC标题
图片|IPTC_Caption|IPTC说明
图片|IPTC_Keywords|IPTC关键词
图片|IPTC_Byline|IPTC作者
图片|IPTC_City|IPTC城市
图片|IPTC_Country|IPTC国家
图片|IPTC_DateCreated|IPTC日期
音频|Audio_Title|曲名
音频|Audio_Artist|艺术家
音频|Audio_Album|专辑
音频|Audio_AlbumArtist|专辑艺术家
音频|Audio_Track|音轨序号
音频|Audio_Disc|碟片序号
音频|Audio_Year|年份
音频|Audio_Genre|流派
音频|Audio_Comment|注释
音频|Audio_Composer|作曲
音频|Audio_Duration|时长
音频|Audio_Bitrate|比特率
音频|Audio_SampleRate|采样率
音频|Audio_Channels|声道数
音频|ID3_Title|ID3曲名
音频|ID3_Artist|ID3艺术家
音频|ID3_Album|ID3专辑
音频|ID3_Track|ID3音轨
音频|ID3_Year|ID3年份
音频|ID3_Genre|ID3流派
音频|ID3_Comment|ID3注释
音频|FLAC_Title|FLAC曲名
音频|FLAC_Artist|FLAC艺术家
音频|FLAC_Album|FLAC专辑
音频|WMA_Title|WMA标题
音频|WMA_Author|WMA作者
视频|Video_Width|视频宽度
视频|Video_Height|视频高度
视频|Video_Duration|视频时长
视频|Video_FrameRate|帧率
视频|Video_FrameCount|帧数
视频|Video_Codec|编码
视频|Video_Title|视频标题
视频|Video_Creator|视频作者
视频|Video_DateCreated|媒体创建时间
视频|AVI_Width|AVI宽度
视频|AVI_Height|AVI高度
视频|AVI_FrameRate|AVI帧率
视频|AVI_Duration|AVI时长
文档|Document_Title|文档标题
文档|Document_Subject|文档主题
文档|Document_Author|文档作者
文档|Document_Keywords|文档关键词
文档|Document_Comments|文档说明
文档|Document_LastAuthor|最后修改作者
文档|Document_Pages|页数
文档|Document_Words|字数
文档|Document_Company|公司
文档|Document_Application|应用程序
文档|Document_DateCreated|文档创建时间
文档|Document_DateModified|文档修改时间
文档|PDF_Title|PDF标题
文档|PDF_Author|PDF作者
文档|PDF_Pages|PDF页数
邮件|Email_Subject|邮件主题
邮件|Email_From|发件人
邮件|Email_To|收件人
邮件|Email_DateSent|发送时间
邮件|Email_MessageID|邮件标识
字体|Font_Name|字体名称
字体|Font_Family|字体家族
字体|Font_Subfamily|字体样式
字体|Font_Version|字体版本
字体|Font_PostScriptName|PostScript名称
种子|Torrent_Name|种子名称
种子|Torrent_Hash|Info SHA-1
种子|Torrent_Size|总大小
种子|Torrent_FileCount|文件数
种子|Torrent_Comment|注释
种子|Torrent_Creator|创建工具
种子|Torrent_DateCreated|创建时间
Android|APK_Package|包名
Android|APK_Label|应用名称
Android|APK_VersionName|版本名
Android|APK_VersionCode|版本码
Android|APK_MinSDK|最低SDK
Android|APK_TargetSDK|目标SDK
Android|APK_Architectures|架构
Android|APK_Permissions|权限
程序|Version_ProductName|产品名称
程序|Version_ProductVersion|产品版本
程序|Version_FileVersion|文件版本
程序|Version_CompanyName|公司名称
程序|Version_Description|文件描述
程序|Version_Copyright|版权
程序|Version_OriginalFilename|原文件名
其他|HTML_Title|HTML标题
其他|Date_Now|当前时间
其他|Clipboard_Text|剪贴板文本
其他|System.Title|Windows标题属性
其他|ExifTool_Custom|自定义ExifTool标签（直接输入标签名）
";
        foreach(string row in data.Split('\n')){string line=row.Trim();if(line.Length>0)rows.Add(line.Split('|'));}return rows.ToArray();
    }
    private sealed class CacheEntry {internal long Stamp,Size;internal Dictionary<string,string> Values;}
    private static readonly Dictionary<string,CacheEntry> Cache=new Dictionary<string,CacheEntry>(StringComparer.OrdinalIgnoreCase);
    private static readonly Dictionary<string,string> HashCache=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
    private static Process tool;private static StreamWriter toolInput;private static int command;
    internal static int CurrentIndex,CurrentCount;internal static string CurrentName="";internal static DateTime Now=DateTime.Now;
    internal static string Normalize(string s){return Regex.Replace(s.ToUpperInvariant(),"[^A-Z0-9]","");}
    internal static string ClipboardText(){try{return System.Windows.Forms.Clipboard.ContainsText()?System.Windows.Forms.Clipboard.GetText():"";}catch(Exception e){throw new IOException("无法读取剪贴板："+e.Message);}}
    internal static string WithoutMarks(string s){var b=new StringBuilder();foreach(char c in s.Normalize(NormalizationForm.FormD)){UnicodeCategory k=CharUnicodeInfo.GetUnicodeCategory(c);if(k!=UnicodeCategory.NonSpacingMark&&k!=UnicodeCategory.SpacingCombiningMark&&k!=UnicodeCategory.EnclosingMark)b.Append(c);}return b.ToString().Normalize(NormalizationForm.FormC);}
    internal static string ToolFile(string name){
        string path=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,name);if(File.Exists(path))return path;
        string resource=name=="PascalRules.exe"?"LiteLaunch.Pascal":"LiteLaunch.ExifTool";
        using(Stream stream=Assembly.GetExecutingAssembly().GetManifestResourceStream(resource)){
            if(stream==null)throw new FileNotFoundException("内置组件缺失："+name);
            if(name=="PascalRules.exe"){string temp=path+"."+Process.GetCurrentProcess().Id+".tmp";using(var dest=File.Create(temp))stream.CopyTo(dest);if(File.Exists(path))File.Delete(temp);else File.Move(temp,path);return path;}
            string home=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"exiftool-13.59");Directory.CreateDirectory(home);string root=Path.GetFullPath(home)+Path.DirectorySeparatorChar;
            using(var zip=new ZipArchive(stream,ZipArchiveMode.Read)){foreach(var e in zip.Entries){string target=Path.GetFullPath(Path.Combine(home,e.FullName));if(!target.StartsWith(root,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("组件压缩包路径无效。");if(e.Name.Length==0){Directory.CreateDirectory(target);continue;}Directory.CreateDirectory(Path.GetDirectoryName(target));if(File.Exists(target)&&new FileInfo(target).Length==e.Length)continue;string temp=target+"."+Process.GetCurrentProcess().Id+".tmp";using(var input=e.Open())using(var output=File.Create(temp))input.CopyTo(output);if(File.Exists(target))File.Delete(target);File.Move(temp,target);}}
            string[] files=Directory.GetFiles(home,"exiftool.exe",SearchOption.AllDirectories);if(files.Length==0)files=Directory.GetFiles(home,"exiftool(-k).exe",SearchOption.AllDirectories);if(files.Length!=1)throw new InvalidDataException("ExifTool启动文件不唯一。");return files[0];
        }
    }
    private static string toolPath;
    private static void StartTool(){
        if(tool!=null&&!tool.HasExited)return;if(toolPath==null)toolPath=ToolFile("exiftool.exe");var start=new ProcessStartInfo(toolPath,"-stay_open True -@ -");start.UseShellExecute=false;start.CreateNoWindow=true;start.RedirectStandardInput=true;start.RedirectStandardOutput=true;start.RedirectStandardError=true;start.StandardOutputEncoding=new UTF8Encoding(false,true);start.StandardErrorEncoding=new UTF8Encoding(false,false);tool=Process.Start(start);tool.ErrorDataReceived+=delegate{};tool.BeginErrorReadLine();toolInput=new StreamWriter(tool.StandardInput.BaseStream,new UTF8Encoding(false));toolInput.AutoFlush=true;
    }
    internal static void Shutdown(){try{if(toolInput!=null){toolInput.WriteLine("-stay_open");toolInput.WriteLine("False");}if(tool!=null&&!tool.WaitForExit(1000))tool.Kill();}catch{}finally{if(toolInput!=null)toolInput.Dispose();toolInput=null;if(tool!=null)tool.Dispose();tool=null;}}
    private static string Text(object value){if(value==null)return "";if(value is string)return (string)value;if(value is IEnumerable){var a=new List<string>();foreach(object item in (IEnumerable)value)a.Add(Convert.ToString(item,CultureInfo.InvariantCulture));return String.Join("; ",a);}return Convert.ToString(value,CultureInfo.InvariantCulture);}
    private static Dictionary<string,string> ReadExif(string path){
        var result=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);if(Directory.Exists(path))return result;
        StartTool();int id=++command;toolInput.WriteLine("-json");toolInput.WriteLine("-G1");toolInput.WriteLine("-s");toolInput.WriteLine("-charset");toolInput.WriteLine("filename=UTF8");toolInput.WriteLine("-api");toolInput.WriteLine("LargeFileSupport=1");toolInput.WriteLine("-d");toolInput.WriteLine("%Y-%m-%dT%H:%M:%S");toolInput.WriteLine(path);toolInput.WriteLine("-execute"+id.ToString(CultureInfo.InvariantCulture));
        var text=new StringBuilder();DateTime deadline=DateTime.UtcNow.AddSeconds(20);
        while(true){Task<string> pending=tool.StandardOutput.ReadLineAsync();int left=(int)Math.Max(0,(deadline-DateTime.UtcNow).TotalMilliseconds);if(!pending.Wait(left)){Shutdown();throw new TimeoutException("元数据读取超时："+Path.GetFileName(path));}string line=pending.Result;if(line==null){Shutdown();throw new IOException("元数据组件提前退出。");}if(line=="{ready"+id+"}")break;text.AppendLine(line);if(text.Length>8*1024*1024)throw new InvalidDataException("元数据结果过大。");}
        object[] values=RenamerRules.Json.Deserialize<object[]>(text.ToString());if(values.Length>0){var o=(Dictionary<string,object>)values[0];foreach(var pair in o){string key=Normalize(pair.Key);string value=Text(pair.Value);result[key]=value;int colon=pair.Key.LastIndexOf(':');if(colon>=0){string plain=Normalize(pair.Key.Substring(colon+1));if(!result.ContainsKey(plain))result[plain]=value;}}}
        return result;
    }
    private static Dictionary<string,string> Raw(string path){
        var info=new FileInfo(path);if(!info.Exists&&Directory.Exists(path))return new Dictionary<string,string>();if(!info.Exists)throw new FileNotFoundException("元数据文件不存在。",path);CacheEntry old;long stamp=info.LastWriteTimeUtc.Ticks,size=info.Length;if(Cache.TryGetValue(path,out old)&&old.Stamp==stamp&&old.Size==size)return old.Values;
        var data=ReadExif(path);ReadStructured(path,data);if(Cache.Count>10000)Cache.Clear();Cache[path]=new CacheEntry{Stamp=stamp,Size=size,Values=data};return data;
    }
    private static string Pick(Dictionary<string,string>data,params string[] keys){foreach(string key in keys){string value;if(data.TryGetValue(Normalize(key),out value)&&value.Length>0)return value;}return "";}
    private static string Hash(string path,string algorithm){
        var info=new FileInfo(path);string key=path+"|"+info.LastWriteTimeUtc.Ticks+"|"+info.Length+"|"+algorithm;string cached;if(HashCache.TryGetValue(key,out cached))return cached;string result;
        using(FileStream input=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read)){
            if(algorithm=="CRC32"){uint crc=0xffffffff;byte[] buffer=new byte[65536];int n;while((n=input.Read(buffer,0,buffer.Length))>0)for(int i=0;i<n;i++){crc^=buffer[i];for(int j=0;j<8;j++)crc=(crc&1)!=0?(crc>>1)^0xedb88320:crc>>1;}result=(crc^0xffffffff).ToString("x8",CultureInfo.InvariantCulture);}
            else using(HashAlgorithm hash=HashAlgorithm.Create(algorithm)){if(hash==null)throw new ArgumentException("不支持的校验算法。");result=BitConverter.ToString(hash.ComputeHash(input)).Replace("-","").ToLowerInvariant();}
        }if(HashCache.Count>10000)HashCache.Clear();HashCache[key]=result;return result;
    }
    internal static string Value(string path,string tag,IDictionary<string,object>o){
        string key=Normalize(tag.Trim(':'));string format=RenamerRules.Get(o,"dateFormat","yyyy-mm-dd hh-nn-ss");FileSystemInfo info=Directory.Exists(path)?(FileSystemInfo)new DirectoryInfo(path):new FileInfo(path);string filename=Path.GetFileName(path),extension=RenamerRules.Ext(filename,info is DirectoryInfo);
        switch(key){
            case "FILENAME":case "FILEBASENAME":return filename.Substring(0,filename.Length-extension.Length);
            case "FILEFILENAME":return filename;
            case "FILEEXT":case "FILEEXTENSION":return extension.TrimStart('.');
            case "FILEFOLDERNAME":case "FILEPARENTNAME":return Path.GetFileName(Path.GetDirectoryName(path));
            case "FILEFOLDERPATH":return Path.GetDirectoryName(path)+Path.DirectorySeparatorChar;
            case "FILEPATH":case "FILEFULLPATH":return path;
            case "FILESIZE":return info is FileInfo?((FileInfo)info).Length.ToString(CultureInfo.InvariantCulture):"0";
            case "FILESIZEKB":return info is FileInfo?(((FileInfo)info).Length/1024.0).ToString("0.###",CultureInfo.InvariantCulture):"0";
            case "FILESIZEMB":return info is FileInfo?(((FileInfo)info).Length/1048576.0).ToString("0.###",CultureInfo.InvariantCulture):"0";
            case "FILEDATECREATED":return FormatDate(RenamerRules.Flag(o,"utc")?info.CreationTimeUtc:info.CreationTime,format);
            case "FILEDATEMODIFIED":return FormatDate(RenamerRules.Flag(o,"utc")?info.LastWriteTimeUtc:info.LastWriteTime,format);
            case "FILEDATEACCESSED":return FormatDate(RenamerRules.Flag(o,"utc")?info.LastAccessTimeUtc:info.LastAccessTime,format);
            case "FILEATTRIBUTES":return info.Attributes.ToString();
            case "FILEINDEX":return (CurrentIndex+1).ToString(CultureInfo.InvariantCulture);
            case "FILECOUNT":return CurrentCount.ToString(CultureInfo.InvariantCulture);
            case "CURRENTNAME":return CurrentName;
            case "DATENOW":case "CURRENTDATE":return FormatDate(Now,format);
            case "CLIPBOARDTEXT":case "CLIPBOARD":return ClipboardText();
            case "FILECRC32":case "CRC32":return Hash(path,"CRC32");case "FILEMD5":case "MD5":return Hash(path,"MD5");case "FILESHA1":case "SHA1":return Hash(path,"SHA1");case "FILESHA256":case "SHA256":return Hash(path,"SHA256");case "FILESHA512":case "SHA512":return Hash(path,"SHA512");
        }
        if(key.StartsWith("VERSION",StringComparison.Ordinal)){
            var version=FileVersionInfo.GetVersionInfo(path);switch(key.Substring(7)){case "PRODUCTNAME":return version.ProductName??"";case "PRODUCTVERSION":return version.ProductVersion??"";case "FILEVERSION":return version.FileVersion??"";case "COMPANYNAME":return version.CompanyName??"";case "DESCRIPTION":return version.FileDescription??"";case "COPYRIGHT":return version.LegalCopyright??"";case "ORIGINALFILENAME":return version.OriginalFilename??"";}
        }
        if(tag.StartsWith("System.",StringComparison.OrdinalIgnoreCase))return ShellProperty(path,tag);
        var data=Raw(path);string result="";
        switch(key){
            case "FILEEXTENSIONDETECTED":result=Pick(data,"FileTypeExtension");break;
            case "IMAGEWIDTH":case "VIDEOWIDTH":case "AVIWIDTH":result=Pick(data,"ImageWidth","SourceImageWidth","ExifImageWidth");break;
            case "IMAGEHEIGHT":case "VIDEOHEIGHT":case "AVIHEIGHT":result=Pick(data,"ImageHeight","SourceImageHeight","ExifImageHeight");break;
            case "IMAGEASPECTRATIO":double w,h;if(Double.TryParse(Pick(data,"ImageWidth"),NumberStyles.Any,CultureInfo.InvariantCulture,out w)&&Double.TryParse(Pick(data,"ImageHeight"),NumberStyles.Any,CultureInfo.InvariantCulture,out h)&&h!=0)result=(w/h).ToString("0.####",CultureInfo.InvariantCulture);break;
            case "IMAGEMEGAPIXELS":result=Pick(data,"Megapixels");break;case "IMAGEBITDEPTH":result=Pick(data,"BitsPerSample","BitDepth");break;case "IMAGECOLORSPACE":result=Pick(data,"ColorSpace","ColorType");break;
            case "EXIFDATETAKEN":case "EXIFDATETIMEORIGINAL":result=Pick(data,"DateTimeOriginal","CreateDate");break;
            case "EXIFDATETIMEDIGITIZED":result=Pick(data,"CreateDate","DateTimeDigitized");break;
            case "EXIFDESCRIPTION":result=Pick(data,"ImageDescription","Description");break;
            case "EXIFEXPOSUREBIAS":result=Pick(data,"ExposureCompensation","ExposureBiasValue");break;
            case "EXIFFOCALLENGTH35MM":result=Pick(data,"FocalLengthIn35mmFormat","FocalLength35efl");break;
            case "IPTCOBJECTNAME":result=Pick(data,"IPTC:ObjectName","ObjectName","Title");break;case "IPTCCAPTION":result=Pick(data,"Caption-Abstract","Description");break;case "IPTCBYLINE":result=Pick(data,"By-line","Artist");break;
            case "AUDIOTRACK":case "ID3TRACK":result=Pick(data,"Track","TrackNumber");break;case "AUDIODISC":result=Pick(data,"DiscNumber","PartOfSet");break;
            case "AUDIOYEAR":case "ID3YEAR":result=Pick(data,"Year","Date","ReleaseDate");break;
            case "AUDIOALBUMARTIST":result=Pick(data,"AlbumArtist","Band");break;
            case "AUDIODURATION":case "VIDEODURATION":case "AVIDURATION":result=Pick(data,"Duration","MediaDuration","TrackDuration");break;
            case "AUDIOBITRATE":result=Pick(data,"AudioBitrate","Bitrate","AvgBitrate");break;case "AUDIOSAMPLERATE":result=Pick(data,"SampleRate","AudioSampleRate");break;case "AUDIOCHANNELS":result=Pick(data,"AudioChannels","NumChannels","Channels");break;
            case "VIDEOFRAMERATE":case "AVIFRAMERATE":result=Pick(data,"VideoFrameRate","FrameRate");break;case "VIDEOFRAMECOUNT":result=Pick(data,"FrameCount","VideoFrameCount");break;case "VIDEOCODEC":result=Pick(data,"CompressorName","VideoCodec","CompressorID");break;case "VIDEOCREATOR":result=Pick(data,"Artist","Creator","Author");break;case "VIDEODATECREATED":result=Pick(data,"MediaCreateDate","CreateDate");break;
            case "DOCUMENTAUTHOR":case "PDFAUTHOR":result=Pick(data,"Document_Author","Author","Creator");break;case "DOCUMENTCOMMENTS":result=Pick(data,"Document_Comments","Description","Comment");break;case "DOCUMENTLASTAUTHOR":result=Pick(data,"LastModifiedBy","LastSavedBy");break;
            case "DOCUMENTPAGES":case "PDFPAGES":result=Pick(data,"Document_Pages","PageCount","Pages");break;case "DOCUMENTWORDS":result=Pick(data,"Words","WordCount");break;case "DOCUMENTAPPLICATION":result=Pick(data,"Application","CreatorTool","Producer");break;
            case "DOCUMENTDATECREATED":result=Pick(data,"Document_DateCreated","CreateDate","Created");break;case "DOCUMENTDATEMODIFIED":result=Pick(data,"Document_DateModified","ModifyDate","Modified");break;
            case "EMAILDATESENT":result=Pick(data,"Email_DateSent","Date","SendDate");break;
            case "FONTNAME":result=Pick(data,"FontName","FullName","FontFullName");break;case "FONTFAMILY":result=Pick(data,"FontFamily","FontFamilyName");break;case "FONTSUBFAMILY":result=Pick(data,"FontSubfamily","FontSubfamilyName");break;case "FONTVERSION":result=Pick(data,"FontVersion","Version");break;case "FONTPOSTSCRIPTNAME":result=Pick(data,"PostScriptName","FontName");break;
            default:
                result=Pick(data,tag);if(result.Length==0){foreach(string prefix in new[]{"EXIF","IPTC","ID3","FLAC","WMA","AUDIO","VIDEO","DOCUMENT","PDF","EMAIL","FONT","EXIFTOOL"})if(key.StartsWith(prefix,StringComparison.Ordinal)){result=Pick(data,key.Substring(prefix.Length));if(result.Length>0)break;}}
                break;
        }
        if(result.Length==0&&key.StartsWith("DOCUMENT",StringComparison.Ordinal)){string property=key.Substring(8);var properties=new Dictionary<string,string>{{"TITLE","System.Title"},{"SUBJECT","System.Subject"},{"AUTHOR","System.Author"},{"KEYWORDS","System.Keywords"},{"COMMENTS","System.Comment"},{"PAGES","System.Document.PageCount"},{"COMPANY","System.Company"}};string p;if(properties.TryGetValue(property,out p))result=ShellProperty(path,p);}
        DateTime date;if(result.Length>0&&(key.IndexOf("DATE",StringComparison.Ordinal)>=0||key.EndsWith("DATETAKEN",StringComparison.Ordinal))&&DateTime.TryParse(result,CultureInfo.InvariantCulture,DateTimeStyles.AllowWhiteSpaces,out date))result=FormatDate(date,format);
        return result;
    }
    internal static string Expand(string pattern,string path,string original,IDictionary<string,object>o){
        string result=RenamerRules.Rx(@"\{(name|num|parent|taken|created|modified|now)(?::([^{}]+))?\}",RegexOptions.IgnoreCase).Replace(pattern,delegate(Match m){string k=m.Groups[1].Value.ToLowerInvariant();if(k=="name")return CurrentName;if(k=="num"){var copy=new Dictionary<string,object>(o);copy["width"]=RenamerRules.Get(o,"width","3");BigInteger start,step;BigInteger.TryParse(RenamerRules.Get(o,"start","1"),out start);BigInteger.TryParse(RenamerRules.Get(o,"step","1"),out step);return RenamerRules.Serial(start+CurrentIndex*step,copy);}string tag=k=="parent"?"File_FolderName":k=="taken"?"EXIF_DateTaken":k=="created"?"File_DateCreated":k=="modified"?"File_DateModified":"Date_Now";var opts=new Dictionary<string,object>(o);if(m.Groups[2].Success)opts["dateFormat"]=m.Groups[2].Value;return Resolve(path,tag,opts);});
        return RenamerRules.Rx(@":([A-Za-z][A-Za-z0-9_.]*):").Replace(result,delegate(Match m){return Resolve(path,m.Groups[1].Value,o);});
    }
    private static string Resolve(string path,string tag,IDictionary<string,object>o){string value=Value(path,tag,o);if(value.Length==0){string missing=RenamerRules.Get(o,"missing","error");if(missing=="error")throw new ArgumentException("缺少元数据："+tag);if(missing=="fallback")value=RenamerRules.Get(o,"fallback");}if(RenamerRules.Flag(o,"sanitize",true))value=Regex.Replace(value,"[\\x00-\\x1f/\\\\:*?\"<>|]","_");return value;}
    private static string ShellProperty(string path,string property){
        object shell=null,folder=null,item=null;try{Type type=Type.GetTypeFromProgID("Shell.Application");if(type==null)return "";shell=Activator.CreateInstance(type);folder=type.InvokeMember("NameSpace",BindingFlags.InvokeMethod,null,shell,new object[]{Path.GetDirectoryName(path)});if(folder==null)return "";item=folder.GetType().InvokeMember("ParseName",BindingFlags.InvokeMethod,null,folder,new object[]{Path.GetFileName(path)});if(item==null)return "";object value=item.GetType().InvokeMember("ExtendedProperty",BindingFlags.InvokeMethod,null,item,new object[]{property});return Text(value);}catch{return "";}finally{foreach(object v in new[]{item,folder,shell})if(v!=null&&Marshal.IsComObject(v))Marshal.FinalReleaseComObject(v);}}
    internal static string DotNetFormat(string format){
        // Delphi/ReNamer date tokens, with uppercase .NET month tokens accepted too.
        var b=new StringBuilder();bool quoted=false;for(int i=0;i<format.Length;){char c=format[i];if(c=='\''){quoted=!quoted;b.Append(c);i++;continue;}if(quoted||!Char.IsLetter(c)){b.Append(c);i++;continue;}int e=i+1;while(e<format.Length&&Char.ToLowerInvariant(format[e])==Char.ToLowerInvariant(c))e++;string t=format.Substring(i,e-i),key=t.ToLowerInvariant();
            if(key=="nn"||key=="n")b.Append(t.Length==1?"m":"mm");else if(c=='M')b.Append(t);else if(key=="m"||key=="mm"||key=="mmm"||key=="mmmm"){bool minute=t.Length<=2&&Regex.IsMatch(format.Substring(0,i),@"[hH]{1,2}[^yMd]*$");b.Append(new string(minute?'m':'M',t.Length));}else if(key=="ddd"||key=="dddd")b.Append(key);else if(key=="ddddd")b.Append("d");else if(key=="dddddd")b.Append("D");else if(key=="h"||key=="hh")b.Append(new string('H',t.Length));else if(key=="z"||key=="zz"||key=="zzz")b.Append(new string('f',t.Length));else if(key=="am"&&e+3<=format.Length&&format.Substring(i,Math.Min(5,format.Length-i)).Equals("am/pm",StringComparison.OrdinalIgnoreCase)){b.Append("tt");e=i+5;}else b.Append(t);i=e;
        }return b.ToString();
    }
    internal static string FormatDate(DateTime date,string format){return date.ToString(DotNetFormat(format),CultureInfo.CurrentCulture);}
    internal static string ReformatDates(string text,IDictionary<string,object>o){
        var culture=(CultureInfo)CultureInfo.CurrentCulture.Clone();string shortMonths=RenamerRules.Get(o,"shortMonths"),longMonths=RenamerRules.Get(o,"longMonths");if(shortMonths.Length>0){var a=shortMonths.Split(',');if(a.Length!=12)throw new ArgumentException("缩写月份须填写12项。");Array.Resize(ref a,13);a[12]="";culture.DateTimeFormat.AbbreviatedMonthNames=a;culture.DateTimeFormat.AbbreviatedMonthGenitiveNames=a;}if(longMonths.Length>0){var a=longMonths.Split(',');if(a.Length!=12)throw new ArgumentException("完整月份须填写12项。");Array.Resize(ref a,13);a[12]="";culture.DateTimeFormat.MonthNames=a;culture.DateTimeFormat.MonthGenitiveNames=a;}
        string[] adjust=RenamerRules.Get(o,"adjust","0,0,0,0,0,0,0").Split(',');if(adjust.Length!=7)throw new ArgumentException("时间调整须为年,月,日,时,分,秒,毫秒。");int[] delta=new int[7];for(int i=0;i<7;i++)if(!Int32.TryParse(adjust[i],out delta[i]))throw new ArgumentException("时间调整值必须为整数。");
        foreach(string source in RenamerRules.Get(o,"inputFormat","yyyy-mm-dd").Split('|')){
            string format=DotNetFormat(source);var pattern=new StringBuilder();bool quoted=false;for(int i=0;i<format.Length;){char c=format[i];if(c=='\''){quoted=!quoted;i++;continue;}if(quoted){pattern.Append(Regex.Escape(c.ToString()));i++;continue;}int end=i+1;while(end<format.Length&&format[end]==c)end++;string t=format.Substring(i,end-i);if(c=='y')pattern.Append(t.Length<=2?"[0-9]{2}":"[0-9]{4}");else if(c=='M'&&t.Length>=3){var names=t.Length==3?culture.DateTimeFormat.AbbreviatedMonthNames:culture.DateTimeFormat.MonthNames;var valid=new List<string>();foreach(string n in names)if(n.Length>0)valid.Add(Regex.Escape(n));pattern.Append("(?:"+String.Join("|",valid)+")");}else if("MdHhmsf".IndexOf(c)>=0)pattern.Append(t.Length==1?"[0-9]{1,2}":"[0-9]{"+t.Length+"}");else pattern.Append(Regex.Escape(t));i=end;}
            string p=pattern.ToString();if(RenamerRules.Flag(o,"whole"))p="(?<![\\p{L}\\p{N}])"+p+"(?![\\p{L}\\p{N}])";
            text=RenamerRules.Rx(p,RegexOptions.IgnoreCase).Replace(text,delegate(Match m){DateTime date;if(!DateTime.TryParseExact(m.Value,format,culture,DateTimeStyles.None,out date))return m.Value;date=date.AddYears(delta[0]).AddMonths(delta[1]).AddDays(delta[2]).AddHours(delta[3]).AddMinutes(delta[4]).AddSeconds(delta[5]).AddMilliseconds(delta[6]);return date.ToString(DotNetFormat(RenamerRules.Get(o,"outputFormat","yyyymmdd")),culture);});
        }return text;
    }
    [DllImport("icu.dll",CharSet=CharSet.Unicode,CallingConvention=CallingConvention.Cdecl)]private static extern IntPtr utrans_openU(string id,int idLength,int direction,string rules,int rulesLength,IntPtr parseError,ref int error);
    [DllImport("icu.dll",CharSet=CharSet.Unicode,CallingConvention=CallingConvention.Cdecl)]private static extern void utrans_transUChars(IntPtr t,StringBuilder text,ref int textLength,int textCapacity,int start,ref int limit,ref int error);
    [DllImport("icu.dll",CallingConvention=CallingConvention.Cdecl)]private static extern void utrans_close(IntPtr t);
    internal static string Transliterate(string s,string id,string map,bool reverse,bool sensitive){
        if(map.Length>0){var pairs=new Dictionary<string,string>(sensitive?StringComparer.Ordinal:StringComparer.OrdinalIgnoreCase);foreach(string line in RenamerRules.Lines(map)){if(line.Length==0||line.StartsWith("#"))continue;int at=line.IndexOf('=');if(at<1)throw new ArgumentException("自定义转写每行须为 原文=替换文。");string a=line.Substring(0,at),b=line.Substring(at+1);if(reverse){string t=a;a=b;b=t;}if(a.Length==0)throw new ArgumentException("转写原文不能为空。");pairs[a]=b;}var keys=new List<string>(pairs.Keys);keys.Sort(delegate(string a,string b){return b.Length.CompareTo(a.Length);});if(keys.Count==0)return s;return RenamerRules.Rx(String.Join("|",Array.ConvertAll(keys.ToArray(),Regex.Escape)),sensitive?RegexOptions.None:RegexOptions.IgnoreCase).Replace(s,delegate(Match m){return pairs[m.Value];});}
        if(id=="Latin-ASCII")return WithoutMarks(s).Replace("ß","ss").Replace("Æ","AE").Replace("æ","ae").Replace("Ø","O").Replace("ø","o").Replace("Ł","L").Replace("ł","l");
        int error=0;IntPtr trans=IntPtr.Zero;try{trans=utrans_openU(id,-1,reverse?1:0,null,0,IntPtr.Zero,ref error);if(trans==IntPtr.Zero||error>0)throw new ArgumentException("无法打开Unicode转写方案："+id);int capacity=Math.Max(256,s.Length*16+64);var text=new StringBuilder(s,capacity);int length=s.Length,limit=length;error=0;utrans_transUChars(trans,text,ref length,capacity,0,ref limit,ref error);if(error>0)throw new ArgumentException("Unicode转写失败，错误码："+error);return text.ToString();}catch(DllNotFoundException){throw new NotSupportedException("系统不提供ICU转写，请使用拉丁去音调或自定义映射。");}catch(EntryPointNotFoundException){throw new NotSupportedException("系统ICU版本不支持转写，请使用自定义映射。");}finally{if(trans!=IntPtr.Zero)utrans_close(trans);}
    }
    private static void ReadStructured(string path,Dictionary<string,string> data){
        string ext=Path.GetExtension(path).ToLowerInvariant();
        if(ext==".docx"||ext==".xlsx"||ext==".pptx"||ext==".odt"||ext==".ods"||ext==".odp"){
            try{using(var input=File.OpenRead(path))using(var zip=new ZipArchive(input,ZipArchiveMode.Read)){foreach(string name in new[]{"docProps/core.xml","docProps/app.xml","meta.xml"}){var e=zip.GetEntry(name);if(e==null||e.Length>2*1024*1024)continue;using(var stream=e.Open()){var settings=new XmlReaderSettings{DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null};using(var reader=XmlReader.Create(stream,settings)){var document=new XmlDocument{XmlResolver=null};document.Load(reader);foreach(XmlNode node in document.SelectNodes("//*")){if(node.ChildNodes.Count!=1||node.FirstChild.NodeType!=XmlNodeType.Text)continue;string k=node.LocalName,value=node.InnerText;data[Normalize(k)]=value;if(k=="creator")data["DOCUMENTAUTHOR"]=value;if(k=="created")data["DOCUMENTDATECREATED"]=value;if(k=="modified")data["DOCUMENTDATEMODIFIED"]=value;}}}}}}catch(InvalidDataException){}
        }
        if(ext==".html"||ext==".htm"){using(var r=new StreamReader(path,true)){char[] c=new char[2*1024*1024];int n=r.Read(c,0,c.Length);var m=RenamerRules.Rx(@"<title\b[^>]*>(.*?)</title>",RegexOptions.IgnoreCase|RegexOptions.Singleline).Match(new string(c,0,n));if(m.Success)data["HTMLTITLE"]=System.Net.WebUtility.HtmlDecode(m.Groups[1].Value.Trim());}}
        if(ext==".eml"){using(var reader=new StreamReader(path,true)){var headers=new StringBuilder();for(int i=0;i<4096;i++){string line=reader.ReadLine();if(String.IsNullOrEmpty(line))break;headers.AppendLine(line);}string text=Regex.Replace(headers.ToString(),"\\r?\\n[ \\t]+"," ");foreach(string key in new[]{"Subject","From","To","Date","Message-ID"}){var m=RenamerRules.Rx("^"+Regex.Escape(key)+@":\s*(.*)$",RegexOptions.Multiline|RegexOptions.IgnoreCase).Match(text);if(m.Success)data[Normalize("Email_"+(key=="Date"?"DateSent":key))]=DecodeHeader(m.Groups[1].Value.Trim());}}}
        if(ext==".torrent")RenamerFormats.ReadTorrent(path,data);
        if(ext==".apk")RenamerFormats.ReadApk(path,data);
    }
    private static string DecodeHeader(string s){return RenamerRules.Rx(@"=\?([^?]+)\?([bq])\?([^?]*)\?=",RegexOptions.IgnoreCase).Replace(s,delegate(Match m){try{byte[] bytes;if(m.Groups[2].Value.Equals("b",StringComparison.OrdinalIgnoreCase))bytes=Convert.FromBase64String(m.Groups[3].Value);else{string q=m.Groups[3].Value.Replace('_',' ');var b=new List<byte>();for(int i=0;i<q.Length;i++){if(q[i]=='='&&i+2<q.Length){b.Add(Byte.Parse(q.Substring(i+1,2),NumberStyles.HexNumber));i+=2;}else b.Add((byte)q[i]);}bytes=b.ToArray();}return Encoding.GetEncoding(m.Groups[1].Value).GetString(bytes);}catch{return m.Value;}});}
}
