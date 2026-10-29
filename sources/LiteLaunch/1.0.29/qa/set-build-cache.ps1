# Keep Zig caches inside this project's versioned workspace build directory.
$liteLaunchCacheRoot = [IO.Path]::GetFullPath("$PSScriptRoot\..\..\..\..\temp\LiteLaunch-build-1.0.29")
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $liteLaunchCacheRoot 'zig-global-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $liteLaunchCacheRoot 'zig-local-cache'