# Read PE resource data without running any code from the executable.
if(-not ('LiteLaunchUpdateResources' -as [type])){Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class LiteLaunchUpdateResources {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr LoadLibraryEx(string path,IntPtr file,uint flags);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern IntPtr FindResource(IntPtr module,IntPtr name,IntPtr type);
 [DllImport("kernel32.dll")] public static extern uint SizeofResource(IntPtr module,IntPtr resource);
 [DllImport("kernel32.dll")] public static extern IntPtr LoadResource(IntPtr module,IntPtr resource);
 [DllImport("kernel32.dll")] public static extern IntPtr LockResource(IntPtr resource);
 [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
}
'@}
function Get-PackageResource([string]$Path,[int]$Id,[int]$Type=10){
 $module=[LiteLaunchUpdateResources]::LoadLibraryEx([IO.Path]::GetFullPath($Path),[IntPtr]::Zero,0x22)
 if($module -eq [IntPtr]::Zero){throw 'Cannot open executable resource data'}
 try{
  $found=[LiteLaunchUpdateResources]::FindResource($module,[IntPtr]$Id,[IntPtr]$Type)
  if($found -eq [IntPtr]::Zero){throw ('Missing resource: '+$Id)}
  $bytes=New-Object byte[] ([LiteLaunchUpdateResources]::SizeofResource($module,$found))
  [Runtime.InteropServices.Marshal]::Copy([LiteLaunchUpdateResources]::LockResource([LiteLaunchUpdateResources]::LoadResource($module,$found)),$bytes,0,$bytes.Length)
  return ,$bytes
 }finally{[void][LiteLaunchUpdateResources]::FreeLibrary($module)}
}
function Get-BytesSHA256([byte[]]$Bytes){
 $sha=[Security.Cryptography.SHA256]::Create()
 try{([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-','')}finally{$sha.Dispose()}
}
