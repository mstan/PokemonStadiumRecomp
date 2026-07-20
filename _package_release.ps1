param(
    [string]$Version = "v0.4.6-beta",
    [string]$BuildDir = "build",
    [string]$DistDir = "dist"
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path $PSScriptRoot).Path
$build = Join-Path $root $BuildDir
$dist = Join-Path $root $DistDir
$stageName = "PokemonStadiumRecomp-$Version-win64"
$stage = Join-Path $dist $stageName
$zip = Join-Path $dist "$stageName.zip"

$n64RuntimeRoot = Join-Path $root "lib\N64ModernRuntime"
$runtimeCacheEntry = Select-String -Path (Join-Path $build "CMakeCache.txt") `
    -Pattern '^N64MODERN_RUNTIME_ROOT:PATH=(.+)$' -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($runtimeCacheEntry -and $runtimeCacheEntry.Line -match '^N64MODERN_RUNTIME_ROOT:PATH=(.+)$') {
    $n64RuntimeRoot = $Matches[1]
}

$exe = Join-Path $build "PokemonStadiumRecomp.exe"
$assets = Join-Path $build "assets"
if (-not (Test-Path -LiteralPath $exe)) { throw "release executable missing: $exe" }
if (-not (Test-Path -LiteralPath $assets)) { throw "recomp-ui assets missing: $assets" }

$distFull = [IO.Path]::GetFullPath($dist).TrimEnd('\') + '\'
$stageFull = [IO.Path]::GetFullPath($stage)
$zipFull = [IO.Path]::GetFullPath($zip)
if (-not $stageFull.StartsWith($distFull, [StringComparison]::OrdinalIgnoreCase) -or
    -not $zipFull.StartsWith($distFull, [StringComparison]::OrdinalIgnoreCase)) {
    throw "refusing to clean release paths outside dist"
}

if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null

Copy-Item -LiteralPath $exe -Destination $stage
foreach ($dll in @("SDL2.dll", "dxcompiler.dll", "dxil.dll")) {
    $source = Join-Path $build $dll
    if (-not (Test-Path -LiteralPath $source)) { throw "required runtime DLL missing: $source" }
    Copy-Item -LiteralPath $source -Destination $stage
}
Copy-Item -LiteralPath $assets -Destination $stage -Recurse

foreach ($doc in @("README.md", "ISSUES.md", "COPYING")) {
    $source = Join-Path $root $doc
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $stage }
}
$notes = Join-Path $root "_release_notes_$Version.md"
if (Test-Path -LiteralPath $notes) {
    Copy-Item -LiteralPath $notes -Destination (Join-Path $stage "RELEASE_NOTES.md")
}

$licenses = Join-Path $stage "licenses"
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
$licenseMap = @{
    "PokemonStadiumRecomp-GPLv3-COPYING.txt" = (Join-Path $root "COPYING")
    "N64ModernRuntime-GPLv3-COPYING.txt" = (Join-Path $n64RuntimeRoot "COPYING")
    "N64Recomp-MIT-LICENSE.txt" = (Join-Path $n64RuntimeRoot "N64Recomp\LICENSE")
    "rt64-MIT-LICENSE.txt" = (Join-Path $root "lib\rt64\LICENSE")
    "Dear-ImGui-LICENSE.txt" = (Join-Path $root "lib\rt64\src\contrib\imgui\LICENSE.txt")
    "GamepadMotionHelpers-LICENSE.txt" = (Join-Path $root "lib\GamepadMotionHelpers\LICENSE")
    "SlotMap-LICENSE.txt" = (Join-Path $root "lib\SlotMap\README.md")
    "fonts-OFL-1.1.txt" = (Join-Path $root "licenses\OFL-1.1.txt")
}
foreach ($entry in $licenseMap.GetEnumerator()) {
    if (Test-Path -LiteralPath $entry.Value) {
        Copy-Item -LiteralPath $entry.Value -Destination (Join-Path $licenses $entry.Key)
    } else {
        Write-Warning "license source missing: $($entry.Value)"
    }
}

$sdlLicense = Get-ChildItem -Path (Join-Path $build "_deps") -Recurse -File -Filter "LICENSE*.txt" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match "sdl" } | Select-Object -First 1
if ($sdlLicense) {
    Copy-Item -LiteralPath $sdlLicense.FullName -Destination (Join-Path $licenses "SDL2-LICENSE.txt")
}

Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal
Write-Host "PACKAGED: $zip"
Get-FileHash -LiteralPath $zip -Algorithm SHA256 | Out-Host
