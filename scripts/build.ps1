param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path "$PSScriptRoot\.."

Push-Location $Root
try {
    cmake -B build -DCMAKE_BUILD_TYPE=$Config
    cmake --build build --config $Config

    $Exe = Join-Path $Root "build\$Config\videogame_cxx.exe"
    if (-not (Test-Path $Exe)) {
        $Exe = Join-Path $Root "build\videogame_cxx.exe"
    }

    Write-Host ""
    Write-Host "Build complete. Run:" -ForegroundColor Green
    Write-Host "  $Exe"
}
finally {
    Pop-Location
}