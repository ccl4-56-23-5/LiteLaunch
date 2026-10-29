using System;
using System.IO;
using System.Linq;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Windows.Forms;

namespace LiteLaunchCursor {
 public static class Ui {
  public static readonly Color Canvas=Color.FromArgb(244,246,250),Surface=Color.White,Line=Color.FromArgb(226,232,240),Ink=Color.FromArgb(31,42,55),Muted=Color.FromArgb(100,116,139),Faint=Color.FromArgb(148,163,184),Alt=Color.FromArgb(247,249,252);
  public static Color Accent=Color.FromArgb(122,106,216);
  public static Color InkOn(Color fill){return (fill.R*21+fill.G*72+fill.B*7)/100<140?Color.White:Color.FromArgb(24,26,38);}
  public static Color Blend(Color a,Color b,int amount){return Color.FromArgb((a.R*(100-amount)+b.R*amount)/100,(a.G*(100-amount)+b.G*amount)/100,(a.B*(100-amount)+b.B*amount)/100);}
  public static Font Font(float size,bool bold){return new Font("Microsoft YaHei UI",size,bold?FontStyle.Bold:FontStyle.Regular,GraphicsUnit.Pixel);}
  public static GraphicsPath Shape(Rectangle box,int radius){GraphicsPath path=new GraphicsPath();int d=Math.Min(radius*2,Math.Min(box.Width,box.Height));if(d<2){path.AddRectangle(box);return path;}path.AddArc(box.Left,box.Top,d,d,180,90);path.AddArc(box.Right-d,box.Top,d,d,270,90);path.AddArc(box.Right-d,box.Bottom-d,d,d,0,90);path.AddArc(box.Left,box.Bottom-d,d,d,90,90);path.CloseFigure();return path;}
  public static void Round(Graphics g,Rectangle box,int radius,Color fill,Color border){using(GraphicsPath path=Shape(box,radius)){using(Brush b=new SolidBrush(fill))g.FillPath(b,path);using(Pen p=new Pen(border))g.DrawPath(p,path);}}
  public static void Text(Graphics g,string value,Font font,Color color,Rectangle box,TextFormatFlags flags){TextRenderer.DrawText(g,value??"",font,box,color,flags|TextFormatFlags.NoPrefix|TextFormatFlags.EndEllipsis);}
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]static extern int GetPrivateProfileInt(string section,string name,int fallback,string path);
  public static float LoadTheme(){string path=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),@"LiteLaunch\LiteLaunch.ini");int i=GetPrivateProfileInt("Panel","Theme",0,path);int[,] colors={{122,106,216},{16,181,165},{242,107,58},{61,111,227},{225,75,133},{30,158,100},{217,154,22},{46,151,201},{150,119,92},{90,107,128}};i=Math.Max(0,Math.Min(9,i));Accent=Color.FromArgb(colors[i,0],colors[i,1],colors[i,2]);return Math.Max(75,Math.Min(200,GetPrivateProfileInt("Panel","ScalePercent",100,path)))/100f;}
 }
 public sealed class AnimationInfo {
  public int Frames=1;public int[] Sequence=new[]{0},Rates=new[]{6};
  public bool Animated{get{return Frames>1;}}
  public int FrameAt(long milliseconds){long sum=0;for(int i=0;i<Sequence.Length;i++)sum+=Math.Max(1,Rates[i])*1000L;long at=(milliseconds*60)%Math.Max(1,sum);for(int i=0;i<Sequence.Length;i++){long length=Math.Max(1,Rates[i])*1000L;if(at<length)return Sequence[i];at-=length;}return 0;}
  public static AnimationInfo Read(string path){
   AnimationInfo info=new AnimationInfo();if(!path.EndsWith(".ani",StringComparison.OrdinalIgnoreCase))return info;
   byte[] data=File.ReadAllBytes(path);if(data.Length<12||data.Length>67108864||System.Text.Encoding.ASCII.GetString(data,0,4)!="RIFF"||System.Text.Encoding.ASCII.GetString(data,8,4)!="ACON")return info;
   int frames=1,steps=1,rate=6;int[] rates=null,sequence=null;
   for(long at=12;at+8<=data.Length;){int index=(int)at;string kind=System.Text.Encoding.ASCII.GetString(data,index,4);uint length=BitConverter.ToUInt32(data,index+4);long next=at+8+length;if(next>data.Length)break;int start=index+8;
    if(kind=="anih"&&length>=36){frames=(int)Math.Min(2048u,BitConverter.ToUInt32(data,start+4));steps=(int)Math.Min(4096u,BitConverter.ToUInt32(data,start+8));rate=(int)Math.Min(600u,BitConverter.ToUInt32(data,start+28));}
    if((kind=="rate"||kind=="seq ")&&length%4==0&&length<=16384){int[] values=new int[length/4];for(int i=0;i<values.Length;i++)values[i]=(int)Math.Min(100000u,BitConverter.ToUInt32(data,start+i*4));if(kind=="rate")rates=values;else sequence=values;}
    at=next+(length%2);
   }
   info.Frames=Math.Max(1,frames);steps=Math.Max(1,steps);info.Sequence=new int[steps];info.Rates=new int[steps];for(int i=0;i<steps;i++){info.Sequence[i]=sequence!=null&&i<sequence.Length&&sequence[i]<info.Frames?sequence[i]:i%info.Frames;info.Rates[i]=Math.Max(1,rates!=null&&i<rates.Length?rates[i]:rate);}return info;
  }
 }
 public sealed class CursorImage:IDisposable {
  IntPtr handle;public AnimationInfo Animation;public string Path;
  public IntPtr Handle{get{return handle;}}
  [DllImport("user32.dll",EntryPoint="LoadCursorFromFileW",CharSet=CharSet.Unicode,ExactSpelling=true,SetLastError=true)]static extern IntPtr Load(string path);
  [DllImport("user32.dll")]static extern bool DestroyCursor(IntPtr cursor);
  [DllImport("user32.dll",SetLastError=true)]static extern bool DrawIconEx(IntPtr dc,int x,int y,IntPtr cursor,int width,int height,uint step,IntPtr brush,uint flags);
  public CursorImage(string path){Path=Engine.Expand(path);handle=Load(Path);if(handle==IntPtr.Zero)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(),"无法预览指针："+path);try{Animation=AnimationInfo.Read(Path);}catch{Animation=new AnimationInfo();}}
  public void Draw(Graphics graphics,Rectangle bounds,long elapsed){if(handle==IntPtr.Zero)return;IntPtr dc=graphics.GetHdc();try{DrawIconEx(dc,bounds.X,bounds.Y,handle,bounds.Width,bounds.Height,(uint)Animation.FrameAt(elapsed),IntPtr.Zero,3);}finally{graphics.ReleaseHdc(dc);}}
  public Bitmap Thumbnail(int size){Bitmap picture=new Bitmap(size,size);using(Graphics g=Graphics.FromImage(picture)){g.Clear(Ui.Surface);Draw(g,new Rectangle(0,0,size,size),0);}return picture;}
  public void Dispose(){if(handle!=IntPtr.Zero){DestroyCursor(handle);handle=IntPtr.Zero;}}
 }
 public sealed class UiButton:Button {
  public bool Primary;public float ScaleFactor=1;bool over;
  public UiButton(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;TabStop=true;}
  protected override void OnMouseEnter(EventArgs e){over=true;Invalidate();base.OnMouseEnter(e);}protected override void OnMouseLeave(EventArgs e){over=false;Invalidate();base.OnMouseLeave(e);}
  protected override void OnPaint(PaintEventArgs e){e.Graphics.Clear(BackColor);e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;Color fill=Primary?Ui.Accent:Ui.Surface;if(over)fill=Primary?Ui.Blend(fill,Color.Black,8):Ui.Blend(Ui.Surface,Ui.Accent,7);if(!Enabled)fill=Primary?Ui.Blend(Ui.Accent,Ui.Surface,65):Ui.Alt;Ui.Round(e.Graphics,new Rectangle(1,1,Width-3,Height-3),(int)(11*ScaleFactor),fill,Primary?fill:Ui.Line);Color ink=Primary?Ui.InkOn(fill):Ui.Ink;if(!Enabled)ink=Ui.Faint;Ui.Text(e.Graphics,Text,Font,ink,ClientRectangle,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine);if(Focused){using(Pen pen=new Pen(Ui.Blend(fill,Primary?Color.White:Ui.Accent,60))){Rectangle r=new Rectangle(4,4,Width-9,Height-9);using(GraphicsPath p=Ui.Shape(r,(int)(8*ScaleFactor)))e.Graphics.DrawPath(pen,p);}}}
 }
 public sealed class SchemeRow:IDisposable {
  public Scheme Scheme;public Bitmap Thumbnail;public bool Current,Animated;
  public void Dispose(){if(Thumbnail!=null)Thumbnail.Dispose();}
  public override string ToString(){return Scheme==null?"":Scheme.Name;}
 }
 public sealed class SchemeList:ListBox {
  public float ScaleFactor=1;public Font RowFont,CaptionFont;
  int S(int value){return (int)Math.Round(value*ScaleFactor);}
  public SchemeList(){DrawMode=DrawMode.OwnerDrawFixed;BorderStyle=BorderStyle.None;IntegralHeight=false;BackColor=Ui.Surface;}
  protected override void OnDrawItem(DrawItemEventArgs e){
   if(e.Index<0||e.Index>=Items.Count)return;SchemeRow row=(SchemeRow)Items[e.Index];Graphics g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;using(Brush brush=new SolidBrush(Ui.Surface))g.FillRectangle(brush,e.Bounds);Rectangle box=new Rectangle(1,e.Bounds.Y+S(3),Width-S(9),ItemHeight-S(7));bool selected=(e.State&DrawItemState.Selected)!=0;if(selected)Ui.Round(g,box,S(11),Ui.Blend(Ui.Surface,Ui.Accent,9),Ui.Blend(Ui.Surface,Ui.Accent,28));
   Rectangle thumb=new Rectangle(S(10),e.Bounds.Y+S(18),S(48),S(48));Ui.Round(g,thumb,S(9),Ui.Surface,Ui.Line);if(row.Thumbnail!=null)g.DrawImage(row.Thumbnail,new Rectangle(thumb.X+S(5),thumb.Y+S(5),S(38),S(38)));
   int x=S(70);Ui.Text(g,row.Scheme.Name,RowFont,row.Scheme.Error==null?Ui.Ink:Ui.Muted,new Rectangle(x,e.Bounds.Y+S(14),Width-x-S(14),S(24)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);
   string caption=row.Scheme.Error!=null?"文件缺失或损坏":row.Current?"当前使用":(row.Animated?"动态指针":"静态指针")+" · "+row.Scheme.Source;
   Ui.Text(g,caption,CaptionFont,row.Scheme.Error!=null?Color.Firebrick:row.Current?Ui.Accent:Ui.Muted,new Rectangle(x,e.Bounds.Y+S(43),Width-x-S(14),S(23)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);
  }
 }
 public sealed class PreviewGallery:ScrollableControl {
  public static readonly string[] RoleNames={"正常选择","帮助选择","后台运行","忙","精确选择","文本选择","手写","不可用","垂直调整","水平调整","对角线调整 ↘","对角线调整 ↗","移动","候选","链接选择","位置选择","人员选择"};
  public float ScaleFactor=1;public Font TitleFont,BodyFont,SmallFont;public Scheme Scheme;public int SelectedRole;public int LoadedCount{get{return images.Count(i=>i!=null);}}
  CursorImage[] images=new CursorImage[17];System.Windows.Forms.Timer timer=new System.Windows.Forms.Timer();ToolTip tips=new ToolTip();long start;int hover=-1;
  int S(int value){return (int)Math.Round(value*ScaleFactor);}int Columns{get{return Math.Max(2,Math.Min(4,(ClientSize.Width-S(12))/S(145)));}}
  [DllImport("user32.dll")]static extern IntPtr SetCursor(IntPtr cursor);
  public PreviewGallery(){BackColor=Ui.Surface;AutoScroll=true;SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);timer.Interval=50;timer.Tick+=(s,e)=>{if(Visible&&images.Any(i=>i!=null&&i.Animation.Animated))Invalidate();};timer.Start();}
  public void ShowScheme(Scheme scheme){foreach(CursorImage image in images)if(image!=null)image.Dispose();images=new CursorImage[17];Scheme=scheme;SelectedRole=0;start=DateTime.UtcNow.Ticks/TimeSpan.TicksPerMillisecond;
   if(scheme!=null)for(int i=0;i<17;i++)if(!String.IsNullOrWhiteSpace(scheme.Paths[i]))try{images[i]=new CursorImage(scheme.Paths[i]);}catch{}
   UpdateScroll();AutoScrollPosition=Point.Empty;Invalidate();
  }
  void UpdateScroll(){AutoScrollMinSize=new Size(0,S(126)+(int)Math.Ceiling(17d/Columns)*S(83)+S(8));}
  protected override void OnResize(EventArgs e){base.OnResize(e);UpdateScroll();Invalidate();}
  protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);Graphics g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;if(Scheme==null){Ui.Text(g,"选择左侧方案，查看真实指针预览",BodyFont,Ui.Faint,ClientRectangle,TextFormatFlags.VerticalCenter|TextFormatFlags.HorizontalCenter);return;}
   int y=AutoScrollPosition.Y,w=ClientSize.Width-S(8);long elapsed=DateTime.UtcNow.Ticks/TimeSpan.TicksPerMillisecond-start;
   Rectangle hero=new Rectangle(1,y+1,w-S(2),S(108));Ui.Round(g,hero,S(12),Ui.Alt,Ui.Alt);CursorImage current=images[SelectedRole];if(current!=null)current.Draw(g,new Rectangle(S(14),y+S(10),S(86),S(86)),elapsed);
   Ui.Text(g,Scheme.Name,TitleFont,Ui.Ink,new Rectangle(S(118),y+S(13),w-S(136),S(29)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);
   string caption=RoleNames[SelectedRole]+" · "+(current!=null&&current.Animation.Animated?"动态 · "+current.Animation.Frames+" 帧":"静态指针");Ui.Text(g,caption,BodyFont,Ui.Muted,new Rectangle(S(118),y+S(45),w-S(130),S(22)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);
   Ui.Text(g,"把鼠标移入此区域，即可试用所选指针",SmallFont,Ui.Muted,new Rectangle(S(118),y+S(73),w-S(130),S(23)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);
   int cols=Columns,gap=S(9),width=(w-(cols-1)*gap)/cols;
   for(int i=0;i<17;i++){Rectangle box=new Rectangle((i%cols)*(width+gap),y+S(126)+(i/cols)*S(83),width,S(73));if(box.Bottom<0||box.Top>ClientSize.Height)continue;bool selected=i==SelectedRole;Ui.Round(g,box,S(10),selected?Ui.Blend(Ui.Surface,Ui.Accent,6):Ui.Surface,selected?Ui.Blend(Ui.Surface,Ui.Accent,45):Ui.Line);if(images[i]!=null)images[i].Draw(g,new Rectangle(box.X+S(12),box.Y+S(15),S(36),S(36)),elapsed);else Ui.Text(g,"—",BodyFont,Ui.Faint,new Rectangle(box.X+S(12),box.Y+S(18),S(36),S(30)),TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter);
    Ui.Text(g,RoleNames[i],BodyFont,Ui.Ink,new Rectangle(box.X+S(57),box.Y+S(14),width-S(65),S(23)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);Ui.Text(g,images[i]==null?"未指定":images[i].Animation.Animated?"动态":"静态",SmallFont,Ui.Muted,new Rectangle(box.X+S(57),box.Y+S(39),width-S(65),S(20)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);
   }
  }
  int RoleAt(Point point){int y=point.Y-AutoScrollPosition.Y;if(y<S(126))return SelectedRole;int width=(ClientSize.Width-S(8)-(Columns-1)*S(9))/Columns;int col=point.X/(width+S(9)),row=(y-S(126))/S(83),index=row*Columns+col;return col<Columns&&index<17?index:-1;}
  protected override void OnMouseMove(MouseEventArgs e){base.OnMouseMove(e);int index=RoleAt(e.Location);if(index!=hover){hover=index;tips.SetToolTip(this,index>=0&&Scheme!=null?(String.IsNullOrWhiteSpace(Scheme.Paths[index])?"本方案未指定此角色，应用时由Windows使用默认指针":Engine.Expand(Scheme.Paths[index])):"");}}
  protected override void OnMouseDown(MouseEventArgs e){base.OnMouseDown(e);int index=RoleAt(e.Location);if(index>=0){SelectedRole=index;Invalidate();}}
  protected override void WndProc(ref Message m){if(m.Msg==0x20&&hover>=0&&images[hover]!=null){SetCursor(images[hover].Handle);m.Result=(IntPtr)1;return;}base.WndProc(ref m);}
  protected override void Dispose(bool disposing){if(disposing){timer.Dispose();tips.Dispose();foreach(CursorImage image in images)if(image!=null)image.Dispose();}base.Dispose(disposing);}
 }
 public class CursorForm:Form {
  public SchemeList SchemeList=new SchemeList();public PreviewGallery Gallery=new PreviewGallery();
  Label status=new Label();UiButton apply=new UiButton(),restore=new UiButton(),refresh=new UiButton();
  List<Scheme> schemes;string backup;float scale;Font titleFont,sectionFont,bodyFont,smallFont;Rectangle leftCard,rightCard;List<SchemeRow> rows=new List<SchemeRow>();
  Func<List<Scheme>> enumerate;ICursorStore store;Func<bool> reload;bool ownIcon;
  int S(int value){return (int)Math.Round(value*scale);}
  internal float UiScale{get{return scale;}}
  public CursorForm():this(Engine.Enumerate,new RegistryStore(),Engine.Reload,Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),@"LiteLaunch\Plugins\cursor-switcher\previous-state.json")){}
  public CursorForm(Func<List<Scheme>> getSchemes,ICursorStore cursorStore,Func<bool> refreshSystem,string backupPath){
   enumerate=getSchemes;store=cursorStore;reload=refreshSystem;backup=backupPath;
   float preference=Ui.LoadTheme(),dpi;using(Graphics g=Graphics.FromHwnd(IntPtr.Zero))dpi=g.DpiX/96f;Rectangle area=Screen.FromPoint(Cursor.Position).WorkingArea;scale=Math.Min(preference*dpi,Math.Min((area.Width-24)/950f,(area.Height-54)/650f));scale=Math.Max(.65f,scale);
   AutoScaleMode=AutoScaleMode.None;BackColor=Ui.Canvas;DoubleBuffered=true;Text="LiteLaunch · 鼠标指针样式";StartPosition=FormStartPosition.CenterScreen;ClientSize=new Size(Math.Min(S(1100),area.Width-24),Math.Min(S(760),area.Height-54));MinimumSize=new Size(Math.Min(S(950),area.Width),Math.Min(S(650),area.Height));
   try{Icon=System.Drawing.Icon.ExtractAssociatedIcon(Application.ExecutablePath);ownIcon=true;}catch{}
   titleFont=Ui.Font(S(23),true);sectionFont=Ui.Font(S(15),true);bodyFont=Ui.Font(S(13),false);smallFont=Ui.Font(S(12),false);Font=bodyFont;
   SchemeList.ScaleFactor=scale;SchemeList.RowFont=Ui.Font(S(14),false);SchemeList.CaptionFont=smallFont;SchemeList.ItemHeight=S(86);SchemeList.SelectedIndexChanged+=(s,e)=>SelectScheme();SchemeList.DoubleClick+=(s,e)=>ApplySelected();
   Gallery.ScaleFactor=scale;Gallery.TitleFont=Ui.Font(S(18),true);Gallery.BodyFont=bodyFont;Gallery.SmallFont=smallFont;
   apply.Text="应用此方案  →";apply.Primary=true;apply.Click+=(s,e)=>ApplySelected();restore.Text="恢复上一套";restore.Click+=(s,e)=>TryAction(()=>{Engine.Restore(store,reload,backup);RefreshSchemes();status.Text="已恢复上一套指针。";});refresh.Text="刷新方案";refresh.Click+=(s,e)=>RefreshSchemes();
   foreach(UiButton button in new[]{apply,restore,refresh}){button.ScaleFactor=scale;button.Font=bodyFont;button.BackColor=button==refresh?Ui.Surface:Ui.Canvas;}
   status.ForeColor=Ui.Muted;status.BackColor=Ui.Canvas;status.Font=bodyFont;
   Controls.AddRange(new Control[]{SchemeList,Gallery,status,apply,restore,refresh});AcceptButton=apply;
   Resize+=(s,e)=>Arrange();RefreshSchemes();Arrange();
  }
  public void RefreshSchemes(){
   string selected=SchemeList.SelectedIndex>=0?((SchemeRow)SchemeList.SelectedItem).Scheme.Name:null;foreach(SchemeRow row in rows)row.Dispose();rows.Clear();schemes=enumerate();SchemeList.Items.Clear();string current=store.Read("").Text;
   foreach(Scheme scheme in schemes){SchemeRow row=new SchemeRow{Scheme=scheme,Current=String.Equals(scheme.Name,current,StringComparison.OrdinalIgnoreCase)};try{using(CursorImage image=new CursorImage(scheme.Paths[0])){row.Thumbnail=image.Thumbnail(S(42));row.Animated=image.Animation.Animated;}}catch{}rows.Add(row);SchemeList.Items.Add(row);}
   int index=schemes.FindIndex(s=>s.Name==selected);if(index<0)index=rows.FindIndex(r=>r.Current);if(index<0&&rows.Count>0)index=0;SchemeList.SelectedIndex=index;if(index<0)Gallery.ShowScheme(null);
   apply.Enabled=index>=0&&schemes[index].Error==null;restore.Enabled=File.Exists(backup);status.Text=schemes.Count==0?"未发现后装方案。安装指针包后点击刷新。":"当前使用："+(String.IsNullOrWhiteSpace(current)?"未命名方案":current)+"。选择方案可先预览，点击应用后才切换。";Invalidate();
  }
  void SelectScheme(){int index=SchemeList.SelectedIndex;apply.Enabled=index>=0&&schemes[index].Error==null;Gallery.ShowScheme(index>=0?schemes[index]:null);if(index>=0&&schemes[index].Error!=null)status.Text=schemes[index].Error;}
  void ApplySelected(){int index=SchemeList.SelectedIndex;if(index<0)return;Scheme scheme=schemes[index];TryAction(()=>{Engine.Apply(scheme,store,Engine.ValidCursor,reload,backup);RefreshSchemes();status.Text="已切换为："+scheme.Name+"。可点击“恢复上一套”撤回。";});}
  void TryAction(Action action){try{action();}catch(Exception e){try{Directory.CreateDirectory(Path.GetDirectoryName(backup));File.AppendAllText(Path.Combine(Path.GetDirectoryName(backup),"cursor.log"),DateTime.Now.ToString("o")+" "+e.ToString()+Environment.NewLine);}catch{}status.Text=e.Message;MessageBox.Show(this,e.Message,"指针切换未完成",MessageBoxButtons.OK,MessageBoxIcon.Warning);}}
  void Arrange(){if(titleFont==null)return;int margin=S(22),top=S(110),bottom=ClientSize.Height-S(90),left=S(300),right=margin+left+S(18);leftCard=new Rectangle(margin,top,left,bottom-top);rightCard=new Rectangle(right,top,ClientSize.Width-margin-right,bottom-top);SchemeList.SetBounds(margin+S(12),top+S(76),left-S(24),Math.Max(S(40),bottom-top-S(91)));Gallery.SetBounds(right+S(16),top+S(60),rightCard.Width-S(32),Math.Max(S(40),rightCard.Height-S(76)));refresh.SetBounds(ClientSize.Width-margin-S(122),S(28),S(122),S(36));restore.SetBounds(margin,ClientSize.Height-S(62),S(136),S(40));apply.SetBounds(ClientSize.Width-margin-S(174),ClientSize.Height-S(64),S(174),S(42));status.SetBounds(margin+S(154),ClientSize.Height-S(66),Math.Max(S(80),ClientSize.Width-S(390)),S(50));Invalidate();}
  protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);Graphics g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;using(Brush b=new SolidBrush(Ui.Surface))g.FillRectangle(b,new Rectangle(0,0,Width,S(90)));Rectangle icon=new Rectangle(S(24),S(23),S(44),S(44));Ui.Round(g,icon,S(13),Ui.Blend(Ui.Surface,Ui.Accent,12),Ui.Blend(Ui.Surface,Ui.Accent,12));if(Icon!=null)g.DrawIcon(Icon,new Rectangle(S(28),S(27),S(36),S(36)));
   Ui.Text(g,"鼠标指针",titleFont,Ui.Ink,new Rectangle(S(82),S(18),Width-S(260),S(31)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);Ui.Text(g,"挑选一种风格，让指针更顺手",bodyFont,Ui.Muted,new Rectangle(S(83),S(49),Width-S(260),S(24)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);
   foreach(Rectangle card in new[]{leftCard,rightCard}){Rectangle shadow=card;shadow.Offset(0,S(2));Ui.Round(g,shadow,S(16),Color.FromArgb(234,238,245),Color.FromArgb(234,238,245));Ui.Round(g,card,S(16),Ui.Surface,Ui.Line);}
   Ui.Text(g,"我的方案",sectionFont,Ui.Ink,new Rectangle(leftCard.X+S(18),leftCard.Y+S(13),S(140),S(27)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);Ui.Text(g,rows.Count+" 套",bodyFont,Ui.Muted,new Rectangle(leftCard.Right-S(68),leftCard.Y+S(14),S(50),S(26)),TextFormatFlags.Right|TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);Ui.Text(g,"仅显示后装方案 · 双击即可应用",smallFont,Ui.Muted,new Rectangle(leftCard.X+S(18),leftCard.Y+S(42),leftCard.Width-S(32),S(23)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);
   Ui.Text(g,"指针预览",sectionFont,Ui.Ink,new Rectangle(rightCard.X+S(18),rightCard.Y+S(13),S(130),S(28)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);Ui.Text(g,"悬停试用 · 点击角色查看动态",smallFont,Ui.Muted,new Rectangle(rightCard.X+S(118),rightCard.Y+S(16),rightCard.Width-S(135),S(24)),TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter);
  }
  protected override void Dispose(bool disposing){if(disposing){foreach(SchemeRow row in rows)row.Dispose();if(SchemeList.RowFont!=null)SchemeList.RowFont.Dispose();if(Gallery.TitleFont!=null)Gallery.TitleFont.Dispose();if(titleFont!=null)titleFont.Dispose();if(sectionFont!=null)sectionFont.Dispose();if(smallFont!=null)smallFont.Dispose();if(ownIcon&&Icon!=null)Icon.Dispose();}base.Dispose(disposing);}
 }
}
