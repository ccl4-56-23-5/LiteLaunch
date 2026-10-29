using System;using System.Runtime.InteropServices;using System.ComponentModel;
public static class RefreshProbe{
 [DllImport("user32.dll",EntryPoint="SystemParametersInfoW",SetLastError=true,ExactSpelling=true)]static extern bool Spi(uint action,uint param,IntPtr value,uint flags);
 [DllImport("user32.dll",SetLastError=true)]static extern IntPtr GetProcessWindowStation();
 [DllImport("user32.dll",EntryPoint="GetUserObjectInformationW",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool UserInfo(IntPtr obj,int index,System.Text.StringBuilder text,int len,out int required);
 public static int Main(){var text=new System.Text.StringBuilder(256);int needed;UserInfo(GetProcessWindowStation(),2,text,512,out needed);Console.WriteLine("station="+text+" session="+System.Diagnostics.Process.GetCurrentProcess().SessionId);foreach(uint f in new uint[]{0,2,3}){bool ok=Spi(0x57,0,IntPtr.Zero,f);int err=Marshal.GetLastWin32Error();Console.WriteLine("W flags="+f+" result="+ok+" error="+err+" "+new Win32Exception(err).Message);}return 0;}
}
