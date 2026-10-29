using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Globalization;
using System.IO.Compression;
using System.Xml;

// Read-only structured format readers. Bounds are checked before every binary access.
internal static class RenamerFormats {
    private static uint U32(byte[]b,int p){if(p<0||p>b.Length-4)throw new InvalidDataException("二进制结构超出范围。");return BitConverter.ToUInt32(b,p);}
    private static ushort U16(byte[]b,int p){if(p<0||p>b.Length-2)throw new InvalidDataException("二进制结构超出范围。");return BitConverter.ToUInt16(b,p);}
    private static string ReadString(byte[]b,ref int p){int n=0;if(p>=b.Length)throw new InvalidDataException();while(p<b.Length&&b[p]!=':'){if(b[p]<'0'||b[p]>'9'||n>16*1024*1024)throw new InvalidDataException("种子字符串无效。");n=checked(n*10+b[p++]-'0');}if(p>=b.Length||n>b.Length-(p+1))throw new InvalidDataException("种子字符串越界。");p++;string s=Encoding.UTF8.GetString(b,p,n);p+=n;return s;}
    private static object Bencode(byte[]b,ref int p,int depth,ref int infoStart,ref int infoEnd){
        if(depth>64||p>=b.Length)throw new InvalidDataException("种子结构无效。");byte kind=b[p];
        if(kind=='i'){int start=++p;while(p<b.Length&&b[p]!='e')p++;if(p==b.Length)throw new InvalidDataException();long n;if(!Int64.TryParse(Encoding.ASCII.GetString(b,start,p-start),NumberStyles.Integer,CultureInfo.InvariantCulture,out n))throw new InvalidDataException();p++;return n;}
        if(kind=='l'){p++;var list=new List<object>();while(p<b.Length&&b[p]!='e')list.Add(Bencode(b,ref p,depth+1,ref infoStart,ref infoEnd));if(p++>=b.Length)throw new InvalidDataException();return list;}
        if(kind=='d'){p++;var dict=new Dictionary<string,object>();while(p<b.Length&&b[p]!='e'){string key=ReadString(b,ref p);int start=p;object value=Bencode(b,ref p,depth+1,ref infoStart,ref infoEnd);if(depth==0&&key=="info"){infoStart=start;infoEnd=p;}dict[key]=value;}if(p++>=b.Length)throw new InvalidDataException();return dict;}
        return ReadString(b,ref p);
    }
    private static string S(Dictionary<string,object>d,string key){object v;return d.TryGetValue(key,out v)?Convert.ToString(v,CultureInfo.InvariantCulture):"";}
    private static string S(Dictionary<string,string>d,string key){string v;return d.TryGetValue(key,out v)?v:"";}
    internal static void ReadTorrent(string path,Dictionary<string,string>data){
        var file=new FileInfo(path);if(file.Length>32*1024*1024)throw new InvalidDataException("种子文件过大。");byte[]b=File.ReadAllBytes(path);int p=0,start=-1,end=-1;var root=Bencode(b,ref p,0,ref start,ref end) as Dictionary<string,object>;if(root==null||start<0||p!=b.Length)throw new InvalidDataException("种子info结构不存在。");var info=root["info"] as Dictionary<string,object>;if(info==null)throw new InvalidDataException();data["TORRENTNAME"]=S(info,"name.utf-8");if(data["TORRENTNAME"].Length==0)data["TORRENTNAME"]=S(info,"name");using(var sha=SHA1.Create())data["TORRENTHASH"]=BitConverter.ToString(sha.ComputeHash(b,start,end-start)).Replace("-","").ToLowerInvariant();
        long size=0;int count=0;object files;if(info.TryGetValue("files",out files)){foreach(object v in (List<object>)files){var f=(Dictionary<string,object>)v;size=checked(size+Convert.ToInt64(f["length"],CultureInfo.InvariantCulture));count++;}}else{size=Convert.ToInt64(info["length"],CultureInfo.InvariantCulture);count=1;}
        data["TORRENTSIZE"]=size.ToString(CultureInfo.InvariantCulture);data["TORRENTFILECOUNT"]=count.ToString(CultureInfo.InvariantCulture);data["TORRENTCOMMENT"]=S(root,"comment");data["TORRENTCREATOR"]=S(root,"created by");object time;if(root.TryGetValue("creation date",out time))data["TORRENTDATECREATED"]=new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc).AddSeconds(Convert.ToDouble(time,CultureInfo.InvariantCulture)).ToLocalTime().ToString("o",CultureInfo.InvariantCulture);
    }
    private static byte[] EntryBytes(ZipArchiveEntry e,int max){if(e==null)return null;if(e.Length>max)throw new InvalidDataException("APK数据块过大。");using(var s=e.Open())using(var memory=new MemoryStream()){s.CopyTo(memory);return memory.ToArray();}}
    private static int Length8(byte[]b,ref int p){if(p>=b.Length)throw new InvalidDataException();int n=b[p++];if((n&128)!=0){if(p>=b.Length)throw new InvalidDataException();n=((n&127)<<8)|b[p++];}return n;}
    private static int Length16(byte[]b,ref int p){int n=U16(b,p);p+=2;if((n&0x8000)!=0){n=((n&0x7fff)<<16)|U16(b,p);p+=2;}return n;}
    private static List<string> StringPool(byte[]b,int start){
        int header=U16(b,start+2),size=checked((int)U32(b,start+4)),count=checked((int)U32(b,start+8));if(count>1000000||start+size>b.Length)throw new InvalidDataException("APK字符串池无效。");bool utf8=(U32(b,start+16)&256)!=0;int baseOffset=checked(start+(int)U32(b,start+20));var list=new List<string>();for(int i=0;i<count;i++){int p=checked(baseOffset+(int)U32(b,start+header+i*4));int chars,n;if(utf8){chars=Length8(b,ref p);n=Length8(b,ref p);if(n>b.Length-p)throw new InvalidDataException();list.Add(Encoding.UTF8.GetString(b,p,n));}else{chars=Length16(b,ref p);n=checked(chars*2);if(n>b.Length-p)throw new InvalidDataException();list.Add(Encoding.Unicode.GetString(b,p,n));}}return list;
    }
    private static string Pool(List<string>pool,uint index){return index==0xffffffff||index>=pool.Count?"":pool[(int)index];}
    private static string AxmlValue(List<string>pool,uint raw,byte type,uint value){if(raw!=0xffffffff)return Pool(pool,raw);if(type==3)return Pool(pool,value);if(type==1)return "@"+value.ToString("x8",CultureInfo.InvariantCulture);if(type==18)return value==0?"false":"true";return value.ToString(CultureInfo.InvariantCulture);}
    private static Dictionary<string,string> Manifest(byte[]b){
        var result=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);if(b.Length>0&&b[0]=='<'){var settings=new XmlReaderSettings{DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null};using(var ms=new MemoryStream(b))using(var reader=XmlReader.Create(ms,settings)){var d=new XmlDocument{XmlResolver=null};d.Load(reader);foreach(XmlNode n in d.SelectNodes("//*"))foreach(XmlAttribute a in n.Attributes)result[n.LocalName+":"+a.LocalName]=a.Value;}return result;}
        var pool=new List<string>();var permissions=new List<string>();int start=U16(b,2);while(start<b.Length){int kind=U16(b,start),header=U16(b,start+2),size=checked((int)U32(b,start+4));if(size<8||start>b.Length-size)throw new InvalidDataException("APK清单块无效。");if(kind==1)pool=StringPool(b,start);if(kind==0x102){int ext=start+header;string node=Pool(pool,U32(b,ext+4));int attributes=ext+U16(b,ext+8),attrSize=U16(b,ext+10),count=U16(b,ext+12);if(attrSize<20)throw new InvalidDataException();for(int i=0;i<count;i++){int at=checked(attributes+i*attrSize);string key=Pool(pool,U32(b,at+4)),value=AxmlValue(pool,U32(b,at+8),b[at+15],U32(b,at+16));result[node+":"+key]=value;if((node=="uses-permission"||node=="uses-permission-sdk-23")&&key=="name")permissions.Add(value);}}start+=size;}result["permissions"]=String.Join("; ",permissions);return result;
    }
    private static string ResourceLabel(byte[]b,uint id){
        List<string>values=null;int p=U16(b,2);while(p<b.Length){int kind=U16(b,p),header=U16(b,p+2),size=checked((int)U32(b,p+4));if(size<8||p>b.Length-size)throw new InvalidDataException();if(kind==1&&values==null)values=StringPool(b,p);if(kind==0x200&&U32(b,p+8)==id>>24){int package=p;int typeOffset=header>=288?(int)U32(b,p+284):0;int q=p+header;while(q<p+size){int k=U16(b,q),h=U16(b,q+2),n=checked((int)U32(b,q+4));if(n<8||q>p+size-n)throw new InvalidDataException();if(k==0x201&&b[q+8]+typeOffset==((id>>16)&255)){int entry=(int)(id&65535),count=checked((int)U32(b,q+12)),entries=q+(int)U32(b,q+16);if(entry<count){uint offset=U32(b,q+h+entry*4);if(offset!=0xffffffff){int e=checked(entries+(int)offset),entrySize=U16(b,e),flags=U16(b,e+2);if((flags&1)==0){int v=e+entrySize;if(b[v+3]==3&&values!=null)return Pool(values,U32(b,v+4));}}}}q+=n;}}p+=size;}return "";
    }
    internal static void ReadApk(string path,Dictionary<string,string>data){
        using(var file=File.OpenRead(path))using(var zip=new ZipArchive(file,ZipArchiveMode.Read)){
            byte[]manifest=EntryBytes(zip.GetEntry("AndroidManifest.xml"),8*1024*1024);if(manifest==null)throw new InvalidDataException("APK缺少AndroidManifest.xml。");var values=Manifest(manifest);string label=S(values,"application:label");uint resource;if(label.StartsWith("@")&&UInt32.TryParse(label.Substring(1),NumberStyles.HexNumber,CultureInfo.InvariantCulture,out resource)){byte[]table=EntryBytes(zip.GetEntry("resources.arsc"),64*1024*1024);if(table!=null)label=ResourceLabel(table,resource);}
            data["APKPACKAGE"]=S(values,"manifest:package");data["APKLABEL"]=label;data["APKVERSIONNAME"]=S(values,"manifest:versionName");data["APKVERSIONCODE"]=S(values,"manifest:versionCode");data["APKMETAMINSDK"]=S(values,"uses-sdk:minSdkVersion");data["APKMINSDK"]=S(values,"uses-sdk:minSdkVersion");data["APKTARGETSDK"]=S(values,"uses-sdk:targetSdkVersion");data["APKPERMISSIONS"]=S(values,"permissions");var arch=new SortedSet<string>(StringComparer.Ordinal);foreach(var e in zip.Entries)if(e.FullName.StartsWith("lib/",StringComparison.Ordinal)){string[]parts=e.FullName.Split('/');if(parts.Length>2&&parts[1].Length>0)arch.Add(parts[1]);}data["APKARCHITECTURES"]=String.Join("; ",arch);
        }
    }
}
