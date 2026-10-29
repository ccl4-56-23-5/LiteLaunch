# Shared presentation for host updates, plugin updates and plugin management.
function Initialize-LiteLaunchUI {
 Add-Type -AssemblyName System.Windows.Forms
 Add-Type -AssemblyName System.Drawing
 [Windows.Forms.Application]::EnableVisualStyles()
 if('LiteLaunchUi.RoundButton' -as [type]){return}
 Add-Type -ReferencedAssemblies System.Windows.Forms,System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Windows.Forms;
namespace LiteLaunchUi {
 public static class Settings {
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern int GetPrivateProfileInt(string s,string k,int d,string p);
 }
 public class RoundButton:Button {
  public Color Accent=Color.FromArgb(122,106,216); public bool Primary; bool hover;
  public RoundButton(){FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;Cursor=Cursors.Hand;SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.AllPaintingInWmPaint,true);}
  protected override void OnMouseEnter(EventArgs e){hover=true;Invalidate();base.OnMouseEnter(e);}
  protected override void OnMouseLeave(EventArgs e){hover=false;Invalidate();base.OnMouseLeave(e);}
  protected override void OnPaint(PaintEventArgs e){var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;g.Clear(Parent==null?Color.White:Parent.BackColor);var r=new Rectangle(1,1,Width-3,Height-3);int d=20;using(var p=new GraphicsPath()){p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.Right-d,r.Y,d,d,270,90);p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.X,r.Bottom-d,d,d,90,90);p.CloseFigure();Color fill=!Enabled?Color.FromArgb(238,242,247):Primary?Accent:hover?Color.FromArgb(245,244,252):Color.White;using(var b=new SolidBrush(fill))g.FillPath(b,p);if(!Primary)using(var pen=new Pen(Color.FromArgb(226,232,240)))g.DrawPath(pen,p);}
   Color ink=!Enabled?Color.FromArgb(148,163,184):Primary?Color.White:Color.FromArgb(31,42,55);TextRenderer.DrawText(g,Text,Font,ClientRectangle,ink,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.SingleLine);if(Focused&&ShowFocusCues){var f=ClientRectangle;f.Inflate(-6,-6);ControlPaint.DrawFocusRectangle(g,f);}}
 }
 public class Card:Panel {
  public Card(){DoubleBuffered=true;BackColor=Color.White;}
  protected override void OnPaint(PaintEventArgs e){var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;g.Clear(Parent==null?Color.FromArgb(244,246,250):Parent.BackColor);int d=30;var r=new Rectangle(1,1,Width-3,Height-3);using(var p=new GraphicsPath()){p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.Right-d,r.Y,d,d,270,90);p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.X,r.Bottom-d,d,d,90,90);p.CloseFigure();using(var b=new SolidBrush(BackColor))g.FillPath(b,p);using(var pen=new Pen(Color.FromArgb(226,232,240)))g.DrawPath(pen,p);}base.OnPaint(e);}
 }
}
'@
}
function Get-LiteLaunchUI {
 Initialize-LiteLaunchUI
 $path=Join-Path $env:LOCALAPPDATA 'LiteLaunch\LiteLaunch.ini'
 $theme=[LiteLaunchUi.Settings]::GetPrivateProfileInt('Panel','Theme',0,$path)
 $palette=@('7A6AD8','10B5A5','F26B3A','3D6FE3','E14B85','1E9E64','D99A16','2E97C9','96775C','5A6B80')
 if($theme -lt 0 -or $theme -ge $palette.Count){$theme=0}
 @{Canvas=[Drawing.ColorTranslator]::FromHtml('#F4F6FA');Surface=[Drawing.Color]::White;Ink=[Drawing.ColorTranslator]::FromHtml('#1F2A37');Muted=[Drawing.ColorTranslator]::FromHtml('#64748B');Line=[Drawing.ColorTranslator]::FromHtml('#E2E8F0');Accent=[Drawing.ColorTranslator]::FromHtml('#'+$palette[$theme]);Font=(New-Object Drawing.Font('Microsoft YaHei UI',10))}
}
function New-LiteLaunchButton([string]$Text,[bool]$Primary=$false){
 $ui=Get-LiteLaunchUI;$button=New-Object LiteLaunchUi.RoundButton;$button.Text=$Text;$button.Primary=$Primary;$button.Accent=$ui.Accent;$button.Font=$ui.Font;return $button
}
function Set-LiteLaunchFormStyle($Form){
 $ui=Get-LiteLaunchUI;$Form.Font=$ui.Font;$Form.BackColor=$ui.Canvas;$Form.ForeColor=$ui.Ink;$Form.AutoScaleMode='Dpi';$Form.StartPosition='CenterScreen'
}
function Format-LiteLaunchVersion($Version){
 $text=[string]$Version;$parts=$text.Split('.');if($parts.Count -eq 4 -and $parts[3] -eq '0'){return ($parts[0..2]-join '.')};return $text
}
function New-LiteLaunchUpdateView([string]$Title,[string]$Subtitle){
 $ui=Get-LiteLaunchUI;$form=New-Object Windows.Forms.Form;Set-LiteLaunchFormStyle $form
 $form.Text='LiteLaunch · '+$Title;$form.ClientSize=New-Object Drawing.Size(520,356);$form.FormBorderStyle='FixedDialog';$form.MaximizeBox=$false;$form.MinimizeBox=$false
 $header=New-Object Windows.Forms.Panel;$header.BackColor=$ui.Surface;$header.SetBounds(0,0,520,78);$form.Controls.Add($header)
 $heading=New-Object Windows.Forms.Label;$heading.Text=$Title;$heading.Font=New-Object Drawing.Font('Microsoft YaHei UI',16,[Drawing.FontStyle]::Bold);$heading.SetBounds(22,14,478,32);$header.Controls.Add($heading)
 $caption=New-Object Windows.Forms.Label;$caption.Text=$Subtitle;$caption.ForeColor=$ui.Muted;$caption.SetBounds(24,49,474,24);$header.Controls.Add($caption)
 $card=New-Object LiteLaunchUi.Card;$card.SetBounds(22,94,476,118);$form.Controls.Add($card)
 $currentLabel=New-Object Windows.Forms.Label;$currentLabel.Text='当前版本';$currentLabel.ForeColor=$ui.Muted;$currentLabel.SetBounds(18,20,104,28);$card.Controls.Add($currentLabel)
 $current=New-Object Windows.Forms.Label;$current.Name='CurrentVersion';$current.Text='读取中…';$current.Font=New-Object Drawing.Font('Microsoft YaHei UI',14,[Drawing.FontStyle]::Bold);$current.SetBounds(140,14,314,36);$card.Controls.Add($current)
 $latestLabel=New-Object Windows.Forms.Label;$latestLabel.Text='最新版本';$latestLabel.ForeColor=$ui.Muted;$latestLabel.SetBounds(18,74,104,28);$card.Controls.Add($latestLabel)
 $latest=New-Object Windows.Forms.Label;$latest.Name='LatestVersion';$latest.Text='检测中…';$latest.ForeColor=$ui.Accent;$latest.Font=$current.Font;$latest.SetBounds(140,68,314,36);$card.Controls.Add($latest)
 $status=New-Object Windows.Forms.Label;$status.Name='UpdateStatus';$status.Text='正在检查更新…';$status.ForeColor=$ui.Muted;$status.SetBounds(24,226,472,50);$form.Controls.Add($status)
 $progress=New-Object Windows.Forms.ProgressBar;$progress.Name='UpdateProgress';$progress.Style='Marquee';$progress.SetBounds(24,281,472,6);$form.Controls.Add($progress)
 $primary=New-LiteLaunchButton '检测中…' $true;$primary.Name='UpdateAction';$primary.Enabled=$false;$primary.SetBounds(302,304,196,36);$form.Controls.Add($primary)
 $close=New-LiteLaunchButton '关闭';$close.SetBounds(22,304,110,36);$close.Add_Click({$this.FindForm().Close()});$form.Controls.Add($close);$form.CancelButton=$close
 [pscustomobject]@{Form=$form;Heading=$heading;Subtitle=$caption;Current=$current;Latest=$latest;Status=$status;Progress=$progress;Primary=$primary;Close=$close;Card=$card;Result=$null;Plan=$null}
}
function Set-LiteLaunchUpdateState($View,[string]$Current,[string]$Latest,[string]$Status,[string]$Action,[bool]$CanUpdate){
 $View.Current.Text=Format-LiteLaunchVersion $Current;$View.Latest.Text=Format-LiteLaunchVersion $Latest;$View.Status.Text=$Status;$View.Primary.Text=$Action;$View.Primary.Enabled=$CanUpdate;$View.Progress.Visible=$false;$View.Form.Refresh()
}
function Show-LiteLaunchMessage([string]$Title,[string]$Message){
 $view=New-LiteLaunchUpdateView $Title '';$view.Card.Visible=$false;$view.Progress.Visible=$false;$view.Primary.Visible=$false;$view.Status.Text=$Message;$view.Status.SetBounds(24,100,472,100);$view.Close.SetBounds(388,214,110,36);$view.Form.ClientSize=New-Object Drawing.Size(520,270)
 try{[void]$view.Form.ShowDialog()}finally{$view.Form.Dispose()}
}
