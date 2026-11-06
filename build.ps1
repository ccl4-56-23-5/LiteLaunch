param([string]$Compiler,[string]$InnoCompiler,[string]$ApplicationExe,[switch]$ReusePluginPackages)
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'sources\LiteLaunch\1.0.32'
& "$source\build-installer.ps1" -Compiler $Compiler -InnoCompiler $InnoCompiler -ApplicationExe $ApplicationExe -ReusePluginPackages:$ReusePluginPackages
