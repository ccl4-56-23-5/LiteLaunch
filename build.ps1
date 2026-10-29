param([string]$Compiler=(Join-Path $PSScriptRoot 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe'),[string]$InnoCompiler='C:\Program Files (x86)\Inno Setup 6\ISCC.exe',[string]$ApplicationExe)
$ErrorActionPreference='Stop'
if(-not $ApplicationExe){
 $payload=Join-Path $PSScriptRoot 'temp\LiteLaunch-installer\payload-1.0.29'
 & "$PSScriptRoot\sources\LiteLaunch\1.0.29\build.ps1" -Compiler $Compiler -OutputDirectory $payload
 $ApplicationExe=Join-Path $payload 'LiteLaunch.exe'
}
& "$PSScriptRoot\sources\LiteLaunch\1.0.29\build-installer.ps1" -InnoCompiler $InnoCompiler -ApplicationExe $ApplicationExe
