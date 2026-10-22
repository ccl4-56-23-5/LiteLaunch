param(
    [string]$Compiler,
    [string]$OutputDirectory,
    [switch]$Force,
    [switch]$Verify
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\set-build-cache.ps1"
$versionDirectory = Split-Path -Parent $PSScriptRoot
$workspaceDirectory = [IO.Path]::GetFullPath((Join-Path $versionDirectory '..\..\..'))
$sourceDirectory = Join-Path $versionDirectory 'vendor\scintilla'
if (-not $Compiler) { $Compiler = Join-Path $workspaceDirectory 'tools\zig\zig-x86_64-windows-0.15.2\zig.exe' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $workspaceDirectory 'temp\LiteLaunch-build-1.0.22\scintilla' }
$Compiler = [IO.Path]::GetFullPath($Compiler)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) { throw "Zig compiler not found: $Compiler" }
if ((Get-Content -LiteralPath (Join-Path $sourceDirectory 'version.txt') -Raw).Trim() -ne '566') { throw 'Expected official Scintilla 5.6.6 sources.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$objectDirectory = Join-Path $OutputDirectory 'objects'
New-Item -ItemType Directory -Path $objectDirectory -Force | Out-Null
$archivePath = Join-Path $OutputDirectory 'libscintilla.a'
$manifestPath = Join-Path $OutputDirectory 'build-manifest.json'
$logPath = Join-Path $OutputDirectory 'build.log'
$win32Names = @('HanjaDic.cxx', 'ListBox.cxx', 'PlatWin.cxx', 'ScintillaWin.cxx', 'SurfaceD2D.cxx', 'SurfaceGDI.cxx')
$sourceFiles = @((Get-ChildItem -LiteralPath (Join-Path $sourceDirectory 'src') -Filter '*.cxx' -File | Sort-Object Name).FullName)
$sourceFiles += @($win32Names | ForEach-Object { Join-Path $sourceDirectory "win32\$_" })
$flags = @('-target', 'x86_64-windows-gnu', '-std=c++17', '-Os', '-DNDEBUG', '-I', (Join-Path $sourceDirectory 'include'), '-I', (Join-Path $sourceDirectory 'src'), '-I', (Join-Path $sourceDirectory 'win32'))
$sourceRecords = @(Get-ChildItem -LiteralPath (Join-Path $sourceDirectory 'src'), (Join-Path $sourceDirectory 'include'), (Join-Path $sourceDirectory 'win32') -File | Where-Object { $_.Extension -in '.cxx', '.h', '.hpp' } | Sort-Object FullName | ForEach-Object {
    [ordered]@{ path = $_.FullName.Substring($sourceDirectory.Length + 1); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
$compilerVersion = (& $Compiler version | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $compilerVersion -ne '0.15.2') { throw "Expected Zig 0.15.2, found $compilerVersion" }
$inputRecord = [ordered]@{ version = '5.6.6'; compilerVersion = $compilerVersion; compilerSha256 = (Get-FileHash -LiteralPath $Compiler -Algorithm SHA256).Hash; flags = $flags; sources = $sourceRecords }
$inputBytes = [Text.Encoding]::UTF8.GetBytes(($inputRecord | ConvertTo-Json -Depth 8 -Compress))
$sha = [Security.Cryptography.SHA256]::Create()
try { $fingerprint = ([BitConverter]::ToString($sha.ComputeHash($inputBytes))).Replace('-', '') } finally { $sha.Dispose() }
$cached = $false
if (-not $Force -and (Test-Path -LiteralPath $archivePath) -and (Test-Path -LiteralPath $manifestPath)) {
    $previous = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $cached = $previous.fingerprint -eq $fingerprint -and $previous.archiveSha256 -eq (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
}
if (-not $cached) {
    Set-Content -LiteralPath $logPath -Encoding UTF8 -Value "Scintilla 5.6.6 / Zig $compilerVersion / static Windows archive / $($sourceFiles.Count) translation units"
    Push-Location $objectDirectory
    try {
        # Zig's COFF driver accepts one -c input per invocation.
        foreach ($sourceFile in $sourceFiles) {
            $objectPath = Join-Path $objectDirectory ([IO.Path]::GetFileNameWithoutExtension($sourceFile) + '.o')
            Write-Host "Compiling $([IO.Path]::GetFileName($sourceFile))"
            & $Compiler c++ @flags -c $sourceFile -o $objectPath 2>&1 | Tee-Object -FilePath $logPath -Append | Write-Host
            if ($LASTEXITCODE -ne 0) { throw "Scintilla compilation failed; see $logPath" }
        }
        $objects = @($sourceFiles | ForEach-Object { Join-Path $objectDirectory ([IO.Path]::GetFileNameWithoutExtension($_) + '.o') })
        foreach ($objectPath in $objects) { if (-not (Test-Path -LiteralPath $objectPath -PathType Leaf)) { throw "Object missing: $objectPath" } }
        & $Compiler ar rcs $archivePath @objects 2>&1 | Tee-Object -FilePath $logPath -Append | Write-Host
        if ($LASTEXITCODE -ne 0) { throw "Scintilla archiving failed; see $logPath" }
    } finally { Pop-Location }
    $manifest = [ordered]@{ fingerprint = $fingerprint; builtUtc = [DateTime]::UtcNow.ToString('o'); sourceUrl = 'https://www.scintilla.org/scintilla566.zip'; sourceArchiveSha256 = 'A0C0CDF1CF226DC6252020CE9A87A939A8B615E65269DB40AC9123B28FA20A9D'; staticSourceCount = $sourceFiles.Count; archivePath = $archivePath; archiveSha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash; input = $inputRecord }
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
}
if ($Verify) {
    $fixtureSource = Join-Path $sourceDirectory 'LiteLaunchFixture.cxx'
    $fixturePath = Join-Path $OutputDirectory 'scintilla-fixture.exe'
    & $Compiler c++ -target x86_64-windows-gnu -std=c++17 -Os $fixtureSource $archivePath -I (Join-Path $sourceDirectory 'include') -lgdi32 -luser32 -limm32 -lole32 -luuid -loleaut32 -ladvapi32 -o $fixturePath
    if ($LASTEXITCODE -ne 0) { throw 'Scintilla fixture compilation failed.' }
    & $fixturePath 2>&1 | Tee-Object -FilePath (Join-Path $OutputDirectory 'fixture.log') | Write-Host
    if ($LASTEXITCODE -ne 0) { throw 'Scintilla fixture verification failed.' }
}
[pscustomobject]@{ Archive = $archivePath; IncludeDirectory = (Join-Path $sourceDirectory 'include'); ConfigurationHeader = (Join-Path $sourceDirectory 'LiteLaunchEditor.h'); Manifest = $manifestPath; Cached = $cached; LinkLibraries = @('gdi32', 'user32', 'imm32', 'ole32', 'uuid', 'oleaut32', 'advapi32') }
