using System;
using System.IO;
using System.Linq;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Web.Script.Serialization;
using LiteLaunchCursor;
public static class CursorLiveTest {
 [DllImport("user32.dll",EntryPoint="LoadCursorW",ExactSpelling=true)]static extern IntPtr LoadCursor(IntPtr instance,IntPtr id);
 [DllImport("user32.dll")]static extern bool DrawIconEx(IntPtr dc,int x,int y,IntPtr icon,int width,int height,uint step,IntPtr brush,uint flags);
 static string Pixels(){using(var image=new Bitmap(64,64)){using(var g=Graphics.FromImage(image)){g.Clear(Color.White);IntPtr dc=g.GetHdc();try{if(!DrawIconEx(dc,0,0,LoadCursor(IntPtr.Zero,(IntPtr)32512),64,64,0,IntPtr.Zero,3))throw new Exception("Cannot read loaded Windows arrow");}finally{g.ReleaseHdc(dc);}}using(var file=new MemoryStream()){image.Save(file,System.Drawing.Imaging.ImageFormat.Png);using(var hash=SHA256.Create())return BitConverter.ToString(hash.ComputeHash(file.ToArray())).Replace("-","");}}}
 public static int Main(string[] args){
  var store=new RegistryStore();var serializer=new JavaScriptSerializer();var before=Engine.Capture(store);string serialized=serializer.Serialize(before),beforePixels=Pixels();bool touched=false,restored=false;string backup=Path.Combine(args[0],"live-private-backup.json");Directory.CreateDirectory(args[0]);File.WriteAllText(Path.Combine(args[0],"before.json"),serialized);
  try{Scheme target=Engine.Enumerate().First(s=>s.Error==null&&!String.Equals(s.Name,store.Read("").Text,StringComparison.OrdinalIgnoreCase));
   touched=true;Engine.Apply(target,store,Engine.ValidCursor,Engine.Reload,backup);
   if(store.Read("").Text!=target.Name)throw new Exception("Applied scheme name was not read back");
   for(int i=0;i<17;i++)if(store.Read(Engine.Roles[i]).Text!=Engine.Expand(target.Paths[i]))throw new Exception("Applied path did not read back: "+Engine.Roles[i]);
   string afterPixels=Pixels();if(afterPixels==beforePixels)throw new Exception("Loaded system arrow did not change");
   Console.WriteLine("PASS real installed scheme applied; all 17 role paths read back; loaded system arrow pixels changed");
   Engine.Restore(store,Engine.Reload,backup);restored=true;
   if(serializer.Serialize(Engine.Capture(store))!=serialized||Pixels()!=beforePixels)throw new Exception("Original cursor state or loaded arrow did not restore exactly");
   Console.WriteLine("PASS original registry values, types and loaded arrow pixels restored exactly");
   File.WriteAllText(Path.Combine(args[0],"live-result.json"),serializer.Serialize(new{oldScheme=before.Values.First(v=>v.Name=="").Text,testScheme=target.Name,appliedRolePathsReadBack=true,loadedArrowPixelsChanged=true,originalValuesAndTypesRestored=true,originalArrowPixelsRestored=true,originalArrowSHA256=beforePixels,testArrowSHA256=afterPixels}));return 0;
  }catch(Exception e){Console.WriteLine(e.ToString());return 1;}
  finally{if(touched&&!restored)try{Engine.Restore(store,Engine.Reload,backup);Console.WriteLine("RESTORED original scheme in finally");}catch(Exception e){Console.WriteLine("RESTORE ERROR "+e);}}
 }
}
