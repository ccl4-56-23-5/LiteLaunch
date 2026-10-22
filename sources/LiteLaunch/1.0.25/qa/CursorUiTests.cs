using System;
using System.IO;
using System.Linq;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;
using LiteLaunchCursor;
public class PrivateCursorForm:CursorForm {
 public PrivateCursorForm(System.Collections.Generic.List<Scheme> schemes,ICursorStore store,string backup):base(()=>schemes,store,()=>true,backup){}
 protected override bool ShowWithoutActivation{get{return true;}}
}
public static class CursorUiTests {
 static int checks;
 static void Check(bool value,string label){if(!value)throw new Exception("FAIL "+label);checks++;Console.WriteLine("PASS "+label);}
 static string Frame(CursorImage image,int elapsed){using(var picture=new Bitmap(64,64)){using(var g=Graphics.FromImage(picture)){g.Clear(Color.White);image.Draw(g,new Rectangle(0,0,64,64),elapsed);}using(var bytes=new MemoryStream()){picture.Save(bytes,ImageFormat.Png);return Convert.ToBase64String(bytes.ToArray());}}}
 [STAThread]public static int Main(string[] args){
  try{Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);string root=args[0];Directory.CreateDirectory(root);var schemes=Engine.Enumerate();Check(schemes.Count>0,"read installed custom schemes without applying them");
   var memory=new MemoryCursorStore();var original=Engine.Capture(new RegistryStore());foreach(var value in original.Values)memory.Write(value);
   using(var form=new PrivateCursorForm(schemes,memory,Path.Combine(root,"previous.json"))){form.StartPosition=FormStartPosition.Manual;form.Location=new Point(-20000,-20000);form.Show();Application.DoEvents();
    Check(form.SchemeList.Items.Count==schemes.Count,"one thumbnail row for every custom scheme");
    for(int i=0;i<schemes.Count;i++){form.SchemeList.SelectedIndex=i;Application.DoEvents();Check(form.Gallery.LoadedCount==schemes[i].Paths.Count(p=>!String.IsNullOrWhiteSpace(p)),"all assigned role images load for scheme "+i);}
    foreach(var item in form.SchemeList.Items){var row=(SchemeRow)item;Check(row.Thumbnail!=null,"scheme list contains a real cursor thumbnail");}
    string oldName=memory.Read("").Text;form.SchemeList.SelectedIndex=schemes.FindIndex(s=>s.Name!=oldName&&s.Error==null);((Button)form.Controls.Cast<Control>().First(c=>c is Button&&c.Text.StartsWith("应用"))).PerformClick();Check(memory.Read("").Text==((SchemeRow)form.SchemeList.SelectedItem).Scheme.Name,"apply button connects selected preview to cursor engine");((Button)form.Controls.Cast<Control>().First(c=>c is Button&&c.Text=="恢复上一套")).PerformClick();Check(memory.Read("").Text==oldName,"restore button connects to saved previous state");
    foreach(var size in new[]{new Size(950,650),new Size(1100,760),new Size(1300,900)}){form.ClientSize=size;Application.DoEvents();foreach(Control c in form.Controls)Check(c.Left>=0&&c.Top>=0&&c.Right<=form.ClientSize.Width&&c.Bottom<=form.ClientSize.Height,"responsive control bounds: "+c.GetType().Name);}
    form.ClientSize=new Size((int)(1100*form.UiScale),(int)(760*form.UiScale));form.SchemeList.SelectedIndex=schemes.FindIndex(s=>s.Paths[0].EndsWith(".ani",StringComparison.OrdinalIgnoreCase));if(form.SchemeList.SelectedIndex<0)form.SchemeList.SelectedIndex=0;Application.DoEvents();
    using(var image=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(image,new Rectangle(Point.Empty,image.Size));image.Save(Path.Combine(root,"cursor-preview.png"),ImageFormat.Png);}
    Check(form.Gallery.LoadedCount>=15,"preview contains at least fifteen real cursor roles");form.Close();
   }
   var animated=schemes.FirstOrDefault(s=>s.Paths[0].EndsWith(".ani",StringComparison.OrdinalIgnoreCase));if(animated!=null){using(var image=new CursorImage(animated.Paths[0])){Check(image.Animation.Animated&&image.Animation.Frames>1,"ANI metadata enables frame animation");Check(image.Animation.FrameAt(0)>=0&&image.Animation.FrameAt(150)<image.Animation.Frames,"animated frame selection stays in bounds");string first=Frame(image,0);bool changed=false;for(int t=50;t<=2500&&!changed;t+=50)changed=Frame(image,t)!=first;Check(changed,"actual native ANI preview pixels animate across frames");}}
   Check(Engine.Reload(),"production reload with SENDCHANGE succeeds for unchanged current scheme");
   Console.WriteLine("RESULT checks="+checks+" failures=0; preview and unchanged-scheme reload only");return 0;
  }catch(Exception e){Console.WriteLine(e.ToString());return 1;}
 }
}
