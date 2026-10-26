using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using Microsoft.Win32;
using LiteLaunchCursor;
public class MemoryCursorStore:ICursorStore {
 public Dictionary<string,SavedValue> Values=new Dictionary<string,SavedValue>(StringComparer.OrdinalIgnoreCase);
 public SavedValue Read(string key){SavedValue v;return Values.TryGetValue(key,out v)?new SavedValue{Name=v.Name,Text=v.Text,Exists=v.Exists,Kind=v.Kind,Number=v.Number}:new SavedValue{Name=key};}
 public void Write(SavedValue v){if(v.Exists)Values[v.Name]=ReadCopy(v);else Values.Remove(v.Name);}
 SavedValue ReadCopy(SavedValue v){return new SavedValue{Name=v.Name,Text=v.Text,Exists=v.Exists,Kind=v.Kind,Number=v.Number};}
}
public static class CursorTests {
 static int checks;
 static void Check(bool value,string message){if(!value)throw new Exception("FAIL "+message);checks++;Console.WriteLine("PASS "+message);}
 static void Reject(Action a,string message){bool rejected=false;try{a();}catch{rejected=true;}Check(rejected,message);}
 public static int Main(string[] args){
  string work=args[0];Directory.CreateDirectory(work);
  string windows=Environment.GetFolderPath(Environment.SpecialFolder.Windows);
  string stock=Path.Combine(windows,@"Cursors\aero_arrow.cur");
  string[] paths=new string[17];for(int i=0;i<paths.Length;i++)paths[i]="";paths[0]=stock;
  Check(Engine.IsStock(paths,windows),"Windows stock scheme hidden even with a custom display name");
  paths[0]=Path.Combine(windows,@"Cursors\MyTheme\aero_arrow.cur");Check(!Engine.IsStock(paths,windows),"third party subdirectory retained");
  paths[0]=Path.Combine(windows,@"Cursors\my_custom_arrow.cur");Check(!Engine.IsStock(paths,windows),"third party file in Windows Cursors root retained");
  paths[0]=Path.Combine(work,"arrow.cur");Check(!Engine.IsStock(paths,windows),"custom installation outside Windows retained");
  string csv="\""+Path.Combine(work,"theme, with comma.cur")+"\","+new string(',',15);
  string[] parsed=Engine.Parse(csv);Check(parsed.Length==17&&parsed[0].Contains("theme, with comma.cur"),"quoted comma and 17 cursor roles parsed");
  parsed=Engine.Parse(String.Join(",",Enumerable.Repeat("",15)));Check(parsed.Length==17&&parsed[16]=="","legacy 15 roles safely expanded");
  Reject(()=>Engine.Parse("bad,csv"),"malformed scheme rejected");Reject(()=>Engine.Parse("\"broken"),"unterminated quoting rejected");
  Scheme scheme=new Scheme{Name="Example",Source="用户安装",Paths=paths};
  Check(Engine.Validate(scheme,p=>true)==null,"installed custom scheme accepted");
  Check(Engine.Validate(scheme,p=>false)!=null,"missing cursor blocks entire apply");
  MemoryCursorStore store=new MemoryCursorStore();store.Write(new SavedValue{Name="",Exists=true,Kind=(int)RegistryValueKind.String,Text="Before"});store.Write(new SavedValue{Name="Scheme Source",Exists=true,Kind=(int)RegistryValueKind.DWord,Number=2});
  int reloads=0;string backup=Path.Combine(work,"previous.json");
  Engine.Apply(scheme,store,p=>true,()=>{reloads++;return true;},backup);
  Check(reloads==1&&store.Read("").Text=="Example","apply updates all roles and reloads once");
  Check(store.Read("Scheme Source").Number==1&&store.Read("Arrow").Kind==(int)RegistryValueKind.ExpandString,"user scheme source and expandable path kinds written");
  Check(File.Exists(backup)&&File.ReadAllText(backup).Contains("Before"),"previous scheme persisted before mutation");
  store.Write(new SavedValue{Name="",Exists=true,Kind=(int)RegistryValueKind.String,Text="Keep"});store.Write(new SavedValue{Name="Arrow",Exists=false});
  reloads=0;Reject(()=>Engine.Apply(scheme,store,p=>true,()=>{reloads++;return reloads!=1;},backup),"reload failure reported");
  Check(store.Read("").Text=="Keep"&&!store.Read("Arrow").Exists&&reloads==2,"failed apply restores values and missing fields then reloads");
  string immutable=File.ReadAllText(backup);Reject(()=>Engine.Apply(scheme,store,p=>false,()=>true,backup),"invalid path rejected before backup or registry writes");
  Check(File.ReadAllText(backup)==immutable&&store.Read("").Text=="Keep","failed validation leaves backup and cursor state unchanged");
  Snapshot snapshot=Engine.Capture(store);Engine.ValidateSnapshot(snapshot);snapshot.Values[0].Name="UnrelatedSetting";Reject(()=>Engine.ValidateSnapshot(snapshot),"restore cannot write unrelated registry values");
  Console.WriteLine("RESULT checks="+checks+" failures=0; real cursor settings not changed");return 0;
 }
}
