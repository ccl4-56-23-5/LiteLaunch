param([Parameter(Mandatory=$true)][string]$Bridge,[Parameter(Mandatory=$true)][string]$Fixture,[Parameter(Mandatory=$true)][string]$Work)
$ErrorActionPreference='Stop'
if([IntPtr]::Size -ne 4){throw 'This test must use 32-bit Windows PowerShell, matching Setup'}
$Work=[IO.Path]::GetFullPath($Work)
New-Item -ItemType Directory -Force -Path $Work|Out-Null
$a=Join-Path $Work 'target';$b=Join-Path $Work 'other-copy'
New-Item -ItemType Directory -Force -Path $a,$b|Out-Null
$target=Join-Path $a 'LiteLaunch.exe';$other=Join-Path $b 'LiteLaunch.exe'
Copy-Item -LiteralPath $Fixture -Destination $target
Copy-Item -LiteralPath $Fixture -Destination $other
$dll=$Bridge.Replace('"','""')
Add-Type @"
using System;using System.Runtime.InteropServices;using System.Text;
public static class ShutdownQA {
 [DllImport(@"$dll",CharSet=CharSet.Unicode,CallingConvention=CallingConvention.Cdecl)]public static extern int LLProbe(string path);
 [DllImport(@"$dll",CharSet=CharSet.Unicode,CallingConvention=CallingConvention.Cdecl)]public static extern int LLClose(string path,uint timeout);
 public delegate bool EnumProc(IntPtr w,IntPtr l);
 [DllImport("user32.dll")]static extern bool EnumWindows(EnumProc callback,IntPtr l);
 [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetClassName(IntPtr w,StringBuilder name,int count);
 [DllImport("user32.dll")]public static extern IntPtr SendMessage(IntPtr w,uint msg,IntPtr wp,IntPtr lp);
 public static IntPtr Window(uint process){IntPtr found=IntPtr.Zero;EnumWindows((w,l)=>{uint pid;GetWindowThreadProcessId(w,out pid);StringBuilder name=new StringBuilder(128);if(pid==process&&GetClassName(w,name,128)>0&&name.ToString()=="LiteLaunch.Native.Panel.v1"){found=w;return false;}return true;},IntPtr.Zero);return found;}
}
"@
$script:checks=0;$children=New-Object Collections.Generic.List[Diagnostics.Process]
function Check($value,[string]$name){if(-not $value){throw ('FAIL '+$name)};$script:checks++;'PASS '+$name}
function Start-Fixture([string]$mode,[string]$path=$target){
 $ready=Join-Path $Work ([guid]::NewGuid().ToString('N')+'.ready')
 $p=Start-Process -FilePath $path -ArgumentList @($mode,('"'+$ready+'"')) -WindowStyle Hidden -PassThru
 $children.Add($p);$until=[datetime]::UtcNow.AddSeconds(8)
 while(-not (Test-Path -LiteralPath $ready) -and [datetime]::UtcNow -lt $until){Start-Sleep -Milliseconds 30}
 if(-not (Test-Path -LiteralPath $ready)){throw ('Private fixture not ready: '+$mode)}
 return $p
}
function Close-Test([string]$path=$target,[uint32]$timeout=3000){
 if([IO.Path]::GetFullPath($path) -notin @($target,$other)){throw 'Refusing to target a non-fixture path'}
 [ShutdownQA]::LLClose($path,$timeout)
}
function Stop-Fixture($process){if(-not $process.HasExited){$process.Kill();[void]$process.WaitForExit(5000)}}
try{
 Check ([ShutdownQA]::LLProbe($target) -eq 0) 'no running target detected'
 Check ((Close-Test) -eq 0) 'no running target needs no shutdown'
 Check ([ShutdownQA]::LLProbe((Join-Path $Work 'missing\LiteLaunch.exe')) -eq 0) 'first install without old executable accepted'
 $wrong=Join-Path $Work 'Different.exe';Copy-Item -LiteralPath $Fixture -Destination $wrong
 Check ([ShutdownQA]::LLProbe($wrong) -eq -2) 'unexpected executable name rejected'
 $p=Start-Fixture 'legacy';$window=[ShutdownQA]::Window($p.Id)
 [void][ShutdownQA]::SendMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
 Check (-not $p.WaitForExit(100) -and [ShutdownQA]::LLProbe($target) -eq 1) 'ordinary WM_CLOSE hides production panel without exiting'
 Check ((Close-Test) -eq 0 -and $p.WaitForExit(1000)) 'existing exit message closes legacy client despite zero acknowledgment'
 $p=Start-Fixture 'cleanup';$watch=[Diagnostics.Stopwatch]::StartNew();$result=Close-Test;$watch.Stop()
 Check ($result -eq 0 -and $p.HasExited -and $watch.ElapsedMilliseconds -ge 500) 'waits for process cleanup after panel window disappears'
 foreach($mode in 'busy','pending','picking','dragging','icon-dragging'){
  $p=Start-Fixture $mode;$window=[ShutdownQA]::Window($p.Id)
  $query=[ShutdownQA]::SendMessage($window,0x11,[IntPtr]::Zero,[IntPtr]1)
  Check ($query -eq [IntPtr]::Zero) ($mode+': Restart Manager query refuses while operation is active')
  Check ((Close-Test) -eq -3 -and -not $p.HasExited) ($mode+': cooperative exit refuses without forcing termination')
  Stop-Fixture $p
 }
 $p=Start-Fixture 'hung';$watch=[Diagnostics.Stopwatch]::StartNew();$result=Close-Test $target 300;$watch.Stop()
 Check ($result -eq -4 -and $watch.ElapsedMilliseconds -lt 2000 -and -not $p.HasExited) 'unresponsive target times out without forced termination'
 Stop-Fixture $p
 $p=Start-Fixture 'no-window'
 Check ([ShutdownQA]::LLProbe($target) -eq 1 -and (Close-Test) -eq -5 -and -not $p.HasExited) 'process without panel blocks replacement instead of being missed'
 Stop-Fixture $p
 $p=Start-Fixture 'ready' $other
 Check ([ShutdownQA]::LLProbe($target) -eq 0 -and (Close-Test) -eq 0 -and -not $p.HasExited) 'same name and window class in another directory are left running'
 Check ((Close-Test $other) -eq 0 -and $p.WaitForExit(1000)) 'exact other-copy path can be shut down when explicitly selected'
 $p=Start-Fixture 'ready';$window=[ShutdownQA]::Window($p.Id)
 Check ([ShutdownQA]::SendMessage($window,0x11,[IntPtr]::Zero,[IntPtr]1) -eq [IntPtr]1 -and -not $p.HasExited) 'Restart Manager query allows idle shutdown without exiting early'
 [void][ShutdownQA]::SendMessage($window,0x16,[IntPtr]::Zero,[IntPtr]1)
 Check (-not $p.WaitForExit(100)) 'canceled shutdown keeps idle panel process running'
 [void][ShutdownQA]::SendMessage($window,0x16,[IntPtr]1,[IntPtr]1)
 Check ($p.WaitForExit(1000)) 'confirmed Restart Manager end session exits production panel'
 Check ([ShutdownQA]::LLProbe($target) -eq 0) 'target process is absent after confirmed exit'
 'RESULT checks='+$script:checks+' failures=0'
 @{checks=$script:checks;failures=0;bridgeBits=32;bridgeSHA256=(Get-FileHash -LiteralPath $Bridge).Hash;privateFixtureOnly=$true;realInstalledApplicationClosed=$false;installerExecuted=$false}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $Work 'result.json') -Encoding UTF8
}finally{foreach($p in $children){Stop-Fixture $p}}
