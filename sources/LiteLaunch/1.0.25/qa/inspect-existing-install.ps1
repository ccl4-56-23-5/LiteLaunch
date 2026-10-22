param([string]$OutputFile)
# Read-only host inspection. This never starts Setup, the app, or an uninstaller.
$ErrorActionPreference='Stop'
if(-not ('LiteLaunch.InstallInspection' -as [type])){
 Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
namespace LiteLaunch {
 public static class InstallInspection {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)]
  static extern IntPtr FindWindow(string className, string windowName);
  [DllImport("user32.dll")]
  static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
  [DllImport("kernel32.dll")]
  static extern IntPtr OpenProcess(uint access, bool inherit, uint processId);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)]
  static extern bool QueryFullProcessImageName(IntPtr process, uint flags, StringBuilder path, ref uint size);
  [DllImport("kernel32.dll")]
  static extern bool CloseHandle(IntPtr handle);
  public static string RunningApplication() {
   IntPtr window=FindWindow("LiteLaunch.Native.Panel.v1", null);
   if(window==IntPtr.Zero) return null;
   uint pid; GetWindowThreadProcessId(window, out pid);
   if(pid==0) return null;
   IntPtr process=OpenProcess(0x1000, false, pid);
   if(process==IntPtr.Zero) return null;
   try {
    uint size=32768; StringBuilder path=new StringBuilder((int)size);
    return QueryFullProcessImageName(process, 0, path, ref size) ? path.ToString() : null;
   } finally { CloseHandle(process); }
  }
 }
}
'@
}
$keyPath='Software\Microsoft\Windows\CurrentVersion\Uninstall\{AA91BC95-36ED-4203-8942-CC6327501020}_is1'
$candidates=@()
$running=[LiteLaunch.InstallInspection]::RunningApplication()
if($running){$candidates+=[pscustomobject]@{Source='running application';Folder=[IO.Path]::GetDirectoryName($running);RegistryVersion=$null}}
foreach($hive in @([Microsoft.Win32.RegistryHive]::CurrentUser,[Microsoft.Win32.RegistryHive]::LocalMachine)){
 foreach($view in @([Microsoft.Win32.RegistryView]::Registry64,[Microsoft.Win32.RegistryView]::Registry32)){
  $base=[Microsoft.Win32.RegistryKey]::OpenBaseKey($hive,$view)
  try {
   $key=$base.OpenSubKey($keyPath)
   if($key){try{
    foreach($valueName in @('InstallLocation','Inno Setup: App Path')){
     $folder=$key.GetValue($valueName)
     if($folder){$candidates+=[pscustomobject]@{Source=($hive.ToString()+'/'+$view.ToString()+'/'+$valueName);Folder=$folder;RegistryVersion=$key.GetValue('DisplayVersion')}}
    }
   }finally{$key.Dispose()}}
  }finally{$base.Dispose()}
 }
}
$valid=@($candidates|ForEach-Object {
 $exe=Join-Path $_.Folder 'LiteLaunch.exe'
 if(Test-Path -LiteralPath $exe -PathType Leaf){
  $file=Get-Item -LiteralPath $exe
  if($file.VersionInfo.ProductName -eq 'LiteLaunch'){
   [pscustomobject]@{Source=$_.Source;Directory=[IO.Path]::GetDirectoryName($file.FullName);ApplicationVersion=$file.VersionInfo.FileVersion;RegistryVersion=$_.RegistryVersion;ApplicationSHA256=(Get-FileHash -LiteralPath $exe).Hash}
  }
 }
})
$result=[pscustomobject]@{ReadOnly=$true;InstallerExecuted=$false;NativeWindowPath=$running;ValidCandidates=$valid;ExpectedUpgradeDirectory=if($valid.Count){$valid[0].Directory}else{$null};Scope='Native window API and live registry inspection; not an installation runtime test'}
if($OutputFile){$result|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $OutputFile -Encoding utf8}
$result
