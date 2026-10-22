using System;using System.Runtime.InteropServices;
public class NativePluginStartQA{
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]public struct SI{public uint cb;public string reserved,desktop,title;public uint x,y,w,h,cx,cy,fill,flags;public short show,res2;public IntPtr reserved2,input,output,error;}
 [StructLayout(LayoutKind.Sequential)]public struct PI{public IntPtr process,thread;public uint pid,tid;}
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool CreateProcess(string exe,string command,IntPtr pa,IntPtr ta,bool inherit,uint flags,IntPtr env,string cwd,ref SI startup,out PI process);
 [DllImport("kernel32.dll")]static extern uint WaitForSingleObject(IntPtr process,uint ms);
 [DllImport("kernel32.dll")]static extern bool CloseHandle(IntPtr handle);
 public static void Run(string exe,string script,string file,short show){SI s=new SI();s.cb=(uint)Marshal.SizeOf(s);s.flags=1;s.show=show;PI p;if(!CreateProcess(exe,"\""+exe+"\" -NoProfile -STA -ExecutionPolicy Bypass -File \""+script+"\" -Result \""+file+"\"",IntPtr.Zero,IntPtr.Zero,false,0x08000000,IntPtr.Zero,null,ref s,out p))throw new Exception("CreateProcess failed");CloseHandle(p.thread);if(WaitForSingleObject(p.process,15000)!=0)throw new Exception("Private child timed out");CloseHandle(p.process);}
}
