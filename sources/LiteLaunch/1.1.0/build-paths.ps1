# Build paths shared by the host, installer and QA. No machine-specific paths are published.
$liteLaunchVersion = '1.1.0'
$liteLaunchRepository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$liteLaunchWorkspace = $liteLaunchRepository
$workspaceCandidate = [IO.Path]::GetFullPath((Join-Path $liteLaunchRepository '..\..\..'))
if ((Split-Path (Split-Path (Split-Path $liteLaunchRepository -Parent) -Parent) -Leaf) -eq 'source' -and
    (Test-Path -LiteralPath (Join-Path $workspaceCandidate 'tool') -PathType Container)) {
    $liteLaunchWorkspace = $workspaceCandidate
}
if ($env:LITELAUNCH_WORKSPACE) { $liteLaunchWorkspace = [IO.Path]::GetFullPath($env:LITELAUNCH_WORKSPACE) }
$liteLaunchBuildRoot = Join-Path $liteLaunchWorkspace '.cache\LiteLaunch\1.1.0'
$liteLaunchOutputRoot = Join-Path $liteLaunchWorkspace 'output\LiteLaunch\1.1.0'
$liteLaunchPayloadRoot = Join-Path $liteLaunchBuildRoot 'payload'
$liteLaunchCompiler = $env:LITELAUNCH_COMPILER
if (-not $liteLaunchCompiler) {
    $compilerCandidates = @(
        (Join-Path $liteLaunchWorkspace 'tool\Win\zig-x86_64-windows-0.17.1\zig.exe'),
        (Join-Path $liteLaunchWorkspace 'tool\Win\zig-x86_64-windows-0.15.2\zig.exe'),
        (Join-Path $liteLaunchRepository 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe')
    )
    $liteLaunchCompiler = $compilerCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (-not $liteLaunchCompiler) { $liteLaunchCompiler = 'zig' }
}
$liteLaunchInnoCompiler = $env:LITELAUNCH_INNO_COMPILER
if (-not $liteLaunchInnoCompiler) {
    $innoCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $liteLaunchWorkspace 'tool\Win\InnoSetup-6.7.4\ISCC.exe'),
        (Join-Path $liteLaunchRepository 'tools\inno\compiler\ISCC.exe')
    )
    $liteLaunchInnoCompiler = $innoCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (-not $liteLaunchInnoCompiler) { $liteLaunchInnoCompiler = 'ISCC' }
}
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $liteLaunchBuildRoot 'zig-global-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $liteLaunchBuildRoot 'zig-local-cache'
