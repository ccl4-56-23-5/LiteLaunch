using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;
using System.Reflection;
[assembly: AssemblyTitle("LiteLaunch Cursor Switcher")]
[assembly: AssemblyProduct("LiteLaunch Cursor Switcher")]
[assembly: AssemblyVersion("1.0.1.0")]
[assembly: AssemblyFileVersion("1.0.1.0")]

namespace LiteLaunchCursor {
 public class Scheme {
  public string Name, Source;
  public string[] Paths;
  public string Error;
  public override string ToString(){return Name;}
 }
 public class SavedValue {public string Name, Text; public bool Exists; public int Kind, Number;}
 public class Snapshot {public int Format=1; public List<SavedValue> Values=new List<SavedValue>();}
 public interface ICursorStore {SavedValue Read(string name); void Write(SavedValue value);}
 public class RegistryStore:ICursorStore {
  public SavedValue Read(string name){
   using(RegistryKey key=Registry.CurrentUser.OpenSubKey(@"Control Panel\Cursors",false)){
    SavedValue value=new SavedValue{Name=name};
    if(key==null || !key.GetValueNames().Contains(name,StringComparer.OrdinalIgnoreCase))return value;
    value.Exists=true; value.Kind=(int)key.GetValueKind(name);
    object data=key.GetValue(name,null,RegistryValueOptions.DoNotExpandEnvironmentNames);
    if(value.Kind==(int)RegistryValueKind.DWord)value.Number=Convert.ToInt32(data);
    else if(value.Kind==(int)RegistryValueKind.String||value.Kind==(int)RegistryValueKind.ExpandString)value.Text=Convert.ToString(data);
    else throw new InvalidOperationException("指针注册值类型不受支持："+name);
    return value;
   }
  }
  public void Write(SavedValue value){
   using(RegistryKey key=Registry.CurrentUser.CreateSubKey(@"Control Panel\Cursors")){
    if(!value.Exists){key.DeleteValue(value.Name,false);return;}
    if(value.Kind==(int)RegistryValueKind.DWord)key.SetValue(value.Name,value.Number,RegistryValueKind.DWord);
    else key.SetValue(value.Name,value.Text??"",(RegistryValueKind)value.Kind);
   }
  }
 }
 public static class Engine {
  public static readonly string[] Roles={"Arrow","Help","AppStarting","Wait","Crosshair","IBeam","NWPen","No","SizeNS","SizeWE","SizeNWSE","SizeNESW","SizeAll","UpArrow","Hand","Pin","Person"};
  static readonly Regex StockFile=new Regex(@"^(?:aero_(?:arrow|busy|helpsel|link|move|ns|ew|nwse|nesw|unavail|up|working|pen|precision|text|person|pin)(?:_[lx]+)?|(?:arrow|help|wait|busy|cross|beam|pen|no|size[1-4]|up|move|hand|person|pin)(?:_(?:i|il|im|l|m|r|rl|rm))?)\.(?:cur|ani)$",RegexOptions.IgnoreCase|RegexOptions.CultureInvariant);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr LoadCursorFromFile(string file);
  [DllImport("user32.dll")] static extern bool DestroyCursor(IntPtr cursor);
  [DllImport("user32.dll",EntryPoint="SystemParametersInfoW",ExactSpelling=true,SetLastError=true)] static extern bool SystemParametersInfo(uint action,uint param,IntPtr value,uint flags);
  public static int LastReloadError {get;private set;}
  // Registry paths have already been persisted. UPDATEINIFILE (1) makes
  // SPI_SETCURSORS fail on the user's Windows build; SENDCHANGE (2) reloads
  // successfully and notifies open applications without rewriting the profile.
  public static bool Reload(){bool ok=SystemParametersInfo(0x57,0,IntPtr.Zero,2);LastReloadError=ok?0:Marshal.GetLastWin32Error();return ok;}
  public static bool ValidCursor(string path){
   if(!File.Exists(path)||(!path.EndsWith(".cur",StringComparison.OrdinalIgnoreCase)&&!path.EndsWith(".ani",StringComparison.OrdinalIgnoreCase)))return false;
   IntPtr cursor=LoadCursorFromFile(path);if(cursor==IntPtr.Zero)return false;DestroyCursor(cursor);return true;
  }
  public static string[] Parse(string csv){
   List<string> fields=new List<string>();StringBuilder value=new StringBuilder();bool quoted=false;
   for(int i=0;i<csv.Length;i++){
    char ch=csv[i];
    if(ch=='"'){if(quoted&&i+1<csv.Length&&csv[i+1]=='"'){value.Append('"');i++;}else quoted=!quoted;}
    else if(ch==','&&!quoted){fields.Add(value.ToString().Trim());value.Clear();}else value.Append(ch);
   }
   if(quoted)throw new FormatException("方案路径引号不完整。");fields.Add(value.ToString().Trim());
   if(fields.Count!=15&&fields.Count!=17)throw new FormatException("方案必须包含15或17个指针角色。");
   while(fields.Count<17)fields.Add("");return fields.ToArray();
  }
  public static string Expand(string path){return String.IsNullOrWhiteSpace(path)?"":Path.GetFullPath(Environment.ExpandEnvironmentVariables(path.Trim()));}
  public static bool IsStock(string[] paths,string windows){
   string folder=Path.GetFullPath(Path.Combine(windows,"Cursors")).TrimEnd('\\');
   foreach(string value in paths){
    if(String.IsNullOrWhiteSpace(value))continue;
    string path;try{path=Expand(value);}catch{return false;}
    if(!String.Equals(Path.GetDirectoryName(path),folder,StringComparison.OrdinalIgnoreCase)||!StockFile.IsMatch(Path.GetFileName(path)))return false;
   }
   return true;
  }
  public static string Validate(Scheme scheme,Func<string,bool> valid){
   if(scheme.Paths==null||scheme.Paths.Length!=17)return "方案角色数量错误。";
   if(String.IsNullOrWhiteSpace(scheme.Paths[0]))return "方案缺少正常选择指针。";
   for(int i=0;i<17;i++)if(!String.IsNullOrWhiteSpace(scheme.Paths[i])){
    try{if(!valid(Expand(scheme.Paths[i])))return "无法读取"+Roles[i]+"指针："+scheme.Paths[i];}
    catch(Exception e){return "指针路径无效："+e.Message;}
   }
   return null;
  }
  public static List<Scheme> Enumerate(){
   List<Scheme> result=new List<Scheme>();HashSet<string> known=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   string windows=Environment.GetFolderPath(Environment.SpecialFolder.Windows);
   foreach(RegistryHive hive in new[]{RegistryHive.CurrentUser,RegistryHive.LocalMachine})
   foreach(RegistryView view in new[]{RegistryView.Registry64,RegistryView.Registry32}){
    try{
     using(RegistryKey root=RegistryKey.OpenBaseKey(hive,view))
     using(RegistryKey key=root.OpenSubKey(hive==RegistryHive.CurrentUser?@"Control Panel\Cursors\Schemes":@"SOFTWARE\Microsoft\Windows\CurrentVersion\Control Panel\Cursors\Schemes")){
      if(key==null)continue;
      foreach(string name in key.GetValueNames()){
       if(String.IsNullOrWhiteSpace(name))continue;
       object raw=key.GetValue(name,null,RegistryValueOptions.DoNotExpandEnvironmentNames);if(!(raw is string))continue;
       try{
        string[] paths=Parse((string)raw);if(IsStock(paths,windows))continue;
        string identity=name+"\0"+String.Join("\0",paths.Select(Expand));if(!known.Add(identity))continue;
        Scheme scheme=new Scheme{Name=name,Source=hive==RegistryHive.CurrentUser?"用户安装":"系统级安装",Paths=paths};
        scheme.Error=Validate(scheme,ValidCursor);result.Add(scheme);
       }catch(FormatException){}catch(ArgumentException){}
      }
     }
    }catch(System.Security.SecurityException){}catch(UnauthorizedAccessException){}
   }
   return result.OrderBy(s=>s.Name,StringComparer.CurrentCultureIgnoreCase).ToList();
  }
  static IEnumerable<string> Names(){return Roles.Concat(new[]{"","Scheme Source"});}
  public static Snapshot Capture(ICursorStore store){Snapshot state=new Snapshot();foreach(string name in Names())state.Values.Add(store.Read(name));return state;}
  public static void ValidateSnapshot(Snapshot state){
   if(state==null||state.Format!=1||state.Values==null||state.Values.Count!=19)throw new FormatException("恢复记录格式错误。");
   HashSet<string> names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   foreach(SavedValue v in state.Values){
    if(v==null||!Names().Contains(v.Name,StringComparer.OrdinalIgnoreCase)||!names.Add(v.Name))throw new FormatException("恢复记录字段错误。");
    if(v.Exists&&v.Kind!=(int)RegistryValueKind.String&&v.Kind!=(int)RegistryValueKind.ExpandString&&v.Kind!=(int)RegistryValueKind.DWord)throw new FormatException("恢复记录类型错误。");
    if(v.Exists&&((v.Name=="Scheme Source")!=(v.Kind==(int)RegistryValueKind.DWord)))throw new FormatException("恢复记录字段类型不匹配。");
   }
  }
  static void WriteSnapshot(ICursorStore store,Snapshot snapshot){ValidateSnapshot(snapshot);foreach(SavedValue v in snapshot.Values)store.Write(v);}
  public static void Save(string file,Snapshot snapshot){
   ValidateSnapshot(snapshot);Directory.CreateDirectory(Path.GetDirectoryName(file));
   string temporary=file+".tmp-"+Guid.NewGuid().ToString("N");
   try{File.WriteAllText(temporary,new JavaScriptSerializer().Serialize(snapshot),new UTF8Encoding(false));if(File.Exists(file))File.Replace(temporary,file,null);else File.Move(temporary,file);}
   finally{if(File.Exists(temporary))File.Delete(temporary);}
  }
  public static void Apply(Scheme scheme,ICursorStore store,Func<string,bool> valid,Func<bool> reload,string backup){
   string error=Validate(scheme,valid);if(error!=null)throw new InvalidOperationException(error);
   Snapshot previous=Capture(store);Save(backup,previous);
   try{
    for(int i=0;i<17;i++)store.Write(new SavedValue{Name=Roles[i],Exists=true,Kind=(int)RegistryValueKind.ExpandString,Text=Expand(scheme.Paths[i])});
    store.Write(new SavedValue{Name="",Exists=true,Kind=(int)RegistryValueKind.String,Text=scheme.Name});
    store.Write(new SavedValue{Name="Scheme Source",Exists=true,Kind=(int)RegistryValueKind.DWord,Number=scheme.Source=="用户安装"?1:2});
    if(!reload())throw new InvalidOperationException("Windows未能刷新指针（系统错误码："+LastReloadError+"），已尝试恢复原方案。");
   }catch{WriteSnapshot(store,previous);reload();throw;}
  }
  public static void Restore(ICursorStore store,Func<bool> reload,string backup){
   Snapshot previous=new JavaScriptSerializer().Deserialize<Snapshot>(File.ReadAllText(backup,Encoding.UTF8));ValidateSnapshot(previous);
   foreach(SavedValue v in previous.Values)if(v.Exists&&Roles.Contains(v.Name)&&!String.IsNullOrWhiteSpace(v.Text)&&!ValidCursor(Expand(v.Text)))throw new InvalidOperationException("上一套指针文件已失效，无法恢复："+v.Text);
   Snapshot current=Capture(store);
   try{WriteSnapshot(store,previous);if(!reload())throw new InvalidOperationException("Windows未能刷新上一套指针。");}
   catch{WriteSnapshot(store,current);reload();throw;}
   Save(backup,current);
  }
 }
 public static class Program {
  [STAThread] public static int Main(string[] args){
   if(args.Length==2&&args[0]=="--list-json"){File.WriteAllText(args[1],new JavaScriptSerializer().Serialize(Engine.Enumerate()),new UTF8Encoding(false));return 0;}
   bool first;using(var mutex=new System.Threading.Mutex(true,@"Local\LiteLaunch.CursorSwitcher.UI",out first)){
    if(!first){IntPtr window=FindWindow(null,"LiteLaunch · 鼠标指针样式");if(window!=IntPtr.Zero){ShowWindow(window,9);SetForegroundWindow(window);}return 0;}
    Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new CursorForm());return 0;
   }
  }
  [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern IntPtr FindWindow(string className,string name);
  [DllImport("user32.dll")]static extern bool ShowWindow(IntPtr window,int command);
  [DllImport("user32.dll")]static extern bool SetForegroundWindow(IntPtr window);
 }
}
