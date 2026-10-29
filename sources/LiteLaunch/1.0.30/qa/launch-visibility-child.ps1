param([string]$Result)
Add-Type -AssemblyName System.Windows.Forms
Add-Type 'using System;using System.Runtime.InteropServices; public static class VisibilityQA{[DllImport("user32.dll")]public static extern bool IsWindowVisible(IntPtr w);}'
$form=New-Object Windows.Forms.Form;$form.Text='LiteLaunch.PrivateVisibilityQA';$form.StartPosition='Manual';$form.Location=New-Object Drawing.Point(-20000,-20000)
$timer=New-Object Windows.Forms.Timer;$timer.Interval=600;$timer.Add_Tick({$timer.Stop();[VisibilityQA]::IsWindowVisible($form.Handle)|Set-Content -LiteralPath $Result;$form.Close()});$timer.Start();[void]$form.ShowDialog();$timer.Dispose();$form.Dispose()
