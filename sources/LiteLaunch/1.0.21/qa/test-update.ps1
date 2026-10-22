$ErrorActionPreference='Stop'
$source=Split-Path -Parent $PSScriptRoot
. "$source\update\github-update.ps1" -LibraryOnly
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$testRoot=Join-Path $workspace ('temp\LiteLaunch-build-1.0.21\update-qa-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot|Out-Null
$script:count=0
function Check($Condition,[string]$Label){if(-not $Condition){throw "FAIL $Label"};$script:count++;"PASS $Label"}
function Reject([scriptblock]$Action,[string]$Label){$caught=$false;try{& $Action|Out-Null}catch{$caught=$true};Check $caught $Label}
Check ((Get-StableVersion 'v1.0.21') -eq (Get-StableVersion '1.0.21.0')) 'version normalization'
Check ((Get-StableVersion 'v1.0.100') -gt (Get-StableVersion '1.0.21')) 'numeric version ordering'
Reject {Get-StableVersion 'v1.0.22-rc1'} 'prerelease tag rejected'
$binary=Join-Path $workspace 'outputs\LiteLaunch\1.0.21\LiteLaunch.exe'
$file=Get-Item -LiteralPath $binary;$hash=(Get-FileHash -LiteralPath $binary).Hash
$release=[pscustomobject]@{tag_name='v1.0.21';draft=$false;prerelease=$false;assets=@([pscustomobject]@{name='LiteLaunch.exe';state='uploaded';size=$file.Length;digest=('sha256:'+$hash);browser_download_url='https://github.com/ccl4-56-23-5/LiteLaunch/releases/download/v1.0.21/LiteLaunch.exe'})}
$asset=Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch'
Assert-Download $binary $asset;Check $true 'release asset hash size product and version accepted'
$release.draft=$true;Reject {Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch'} 'draft rejected';$release.draft=$false
$release.assets[0].digest=$null;Reject {Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch'} 'missing server digest rejected';$release.assets[0].digest='sha256:'+$hash
$release.assets[0].browser_download_url='https://github.com/other/repo/download/LiteLaunch.exe';Reject {Get-ReleaseAsset $release 'ccl4-56-23-5/LiteLaunch'} 'foreign repository download rejected'
$asset.SHA256='0'*64;Reject {Assert-Download $binary $asset} 'tampered hash rejected';$asset.SHA256=$hash
$asset.Version=[version]'1.0.22.0';Reject {Assert-Download $binary $asset} 'mismatched EXE version rejected'
$target=Join-Path $testRoot 'target.exe';$package=Join-Path $testRoot 'package.exe'
[IO.File]::WriteAllText($target,'old executable');[IO.File]::WriteAllText($package,'new executable');$expected=(Get-FileHash -LiteralPath $package).Hash
$backup=Install-Package $package $target $expected
Check (([IO.File]::ReadAllText($target)) -eq 'new executable' -and ([IO.File]::ReadAllText($backup)) -eq 'old executable') 'installation replaces executable and retains rollback'
$second=Join-Path $testRoot 'second.exe';[IO.File]::WriteAllText($second,'next')
Reject {Install-Package $second $target (Get-FileHash -LiteralPath $second).Hash} 'existing rollback backup prevents overwrite'
$target=Join-Path $testRoot 'rollback-target.exe';$package=Join-Path $testRoot 'rollback-package.exe';[IO.File]::WriteAllText($target,'old');[IO.File]::WriteAllText($package,'new');$expected=(Get-FileHash -LiteralPath $package).Hash
function Move-Item {param([string]$LiteralPath,[string]$Destination,[switch]$Force);if($LiteralPath -eq $package -and $Destination -eq $target){throw 'injected package move failure'};Microsoft.PowerShell.Management\Move-Item @PSBoundParameters}
try{Reject {Install-Package $package $target $expected} 'failed replacement reports error';Check (([IO.File]::ReadAllText($target)) -eq 'old') 'failed replacement restores original executable'}finally{Remove-Item Function:\Move-Item}
Add-Type -TypeDefinition @"
using System;using System.Runtime.InteropServices;
public static class LiteLaunchResourceQA21 {
[DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr LoadLibraryExW(string path,IntPtr file,uint flags);
[DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindResourceW(IntPtr module,IntPtr name,IntPtr type);
[DllImport("kernel32.dll")] public static extern uint SizeofResource(IntPtr module,IntPtr resource);
[DllImport("kernel32.dll")] public static extern IntPtr LoadResource(IntPtr module,IntPtr resource);
[DllImport("kernel32.dll")] public static extern IntPtr LockResource(IntPtr resource);
[DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
}
"@
$module=[LiteLaunchResourceQA21]::LoadLibraryExW($binary,[IntPtr]::Zero,0x22)
Check ($module -ne [IntPtr]::Zero) 'final EXE opens as data without execution'
try{
 $resource=[LiteLaunchResourceQA21]::FindResourceW($module,[IntPtr]302,[IntPtr]10)
 $size=[LiteLaunchResourceQA21]::SizeofResource($module,$resource)
 $memory=[LiteLaunchResourceQA21]::LockResource([LiteLaunchResourceQA21]::LoadResource($module,$resource))
 $bytes=New-Object byte[] $size;[Runtime.InteropServices.Marshal]::Copy($memory,$bytes,0,$size)
 $sha=[Security.Cryptography.SHA256]::Create()
 try{$embedded=[BitConverter]::ToString($sha.ComputeHash($bytes));$original=[BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes("$source\update\github-update.ps1")))}finally{$sha.Dispose()}
 Check ($embedded -eq $original) 'final embedded updater matches the tested script exactly'
}finally{[void][LiteLaunchResourceQA21]::FreeLibrary($module)}
"RESULT checks=$script:count failures=0"