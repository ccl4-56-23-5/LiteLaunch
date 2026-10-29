$ErrorActionPreference='Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$source=Split-Path -Parent $PSScriptRoot
$workspace=[IO.Path]::GetFullPath("$source\..\..\..")
$build=Join-Path $workspace 'temp\LiteLaunchBatchRenamer-build-1.0.4\rules-qa'
New-Item -ItemType Directory -Path $build -Force|Out-Null
& "$PSScriptRoot\build-ever-regex.ps1" -OutputDirectory $build|Out-Null
$compiler=Join-Path $workspace 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe'
$exe=Join-Path $build 'rules-test.exe'
& $compiler c++ -target x86_64-windows-gnu -std=c++17 -Os -municode -DEVER_REGEX_TESTING "$source\ever_rules_test.cpp" "$source\ever_rules.cpp" "$source\ever_regex.cpp" -o $exe -lole32 -loleaut32 -luuid -lshell32 -lpropsys -ladvapi32 -luser32
if($LASTEXITCODE -ne 0){throw 'Rule tests compile failed'}
Add-Type -AssemblyName System.Drawing
$bitmap=New-Object Drawing.Bitmap(8,8);$stream=New-Object IO.MemoryStream
try {$bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Jpeg);$jpeg=$stream.ToArray()}finally{$bitmap.Dispose();$stream.Dispose()}
$tiff=[byte[]](0x49,0x49,0x2a,0,8,0,0,0,1,0,0x69,0x87,4,0,1,0,0,0,26,0,0,0,0,0,0,0,1,0,3,0x90,2,0,20,0,0,0,44,0,0,0,0,0,0,0)+[Text.Encoding]::ASCII.GetBytes("2020:01:02 03:04:05`0")
$payload=[Text.Encoding]::ASCII.GetBytes("Exif`0`0")+$tiff;$length=$payload.Length+2
$photo=Join-Path $build 'photo.jpg'
[IO.File]::WriteAllBytes($photo,[byte[]](@(0xff,0xd8,0xff,0xe1,($length -shr 8),($length -band 255))+$payload+$jpeg[2..($jpeg.Length-1)]))
$previousTemp=$env:TEMP;$previousTmp=$env:TMP
try{$env:TEMP=$build;$env:TMP=$build;& $exe (Join-Path $build 'EverRegex.exe') '' $photo|Tee-Object -FilePath (Join-Path $build 'rules.log');if($LASTEXITCODE -ne 0){throw 'Rules failed'}}finally{$env:TEMP=$previousTemp;$env:TMP=$previousTmp}