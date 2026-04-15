# ============================================================================
# Build script for Windows (Win32/MinGW)
# Usage: .\scripts\build-windows-mingw.ps1 [Release|Debug]
# ============================================================================

param(
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "============================================"
Write-Host "  Building pcode-editor for Windows (MinGW)"
Write-Host "============================================"
Write-Host "Build type: $BuildType"
Write-Host ""

# Check if cmake is available
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: cmake not found in PATH"
    Write-Host "Install from: https://cmake.org/download/"
    exit 1
}

# Check if make is available
if (-not (Get-Command make -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: make not found in PATH"
    Write-Host "Install MinGW-w64 from: https://www.mingw-w64.org/"
    exit 1
}

# Clean and configure
Write-Host "Configuring CMake..."
$BuildDir = "build-windows-mingw"
if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null
Set-Location $BuildDir

cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=$BuildType
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: CMake configuration failed"
    Set-Location ..
    exit 1
}

# Build
Write-Host "Building..."
$ProcessorCount = [Environment]::ProcessorCount
make -j$ProcessorCount
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Build failed"
    Set-Location ..
    exit 1
}

# Verify
Write-Host ""
Write-Host "============================================"
if (Test-Path "pcode-editor.exe") {
    Write-Host "✅ Build successful!" -ForegroundColor Green
    Write-Host "📍 Executable: $(Get-Location)\pcode-editor.exe"
    Get-Item "pcode-editor.exe" | Select-Object Length, LastWriteTime
} else {
    Write-Host "❌ ERROR: Build failed - executable not found" -ForegroundColor Red
    Set-Location ..
    exit 1
}
Write-Host "============================================"

Set-Location ..
