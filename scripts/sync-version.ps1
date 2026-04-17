#!/usr/bin/env pwsh
# ============================================================================
# Sync version: Reads VERSION file - get_version() reads from file directly
# No code update needed
# ============================================================================

$VERSION_FILE = "VERSION"

if (!(Test-Path $VERSION_FILE)) {
    Write-Host "❌ VERSION file not found"
    exit 1
}
$version = Get-Content $VERSION_FILE -Raw | ForEach-Object { $_.Trim() }
Write-Host "📌 Version from VERSION file: $version"
Write-Host "✅ get_version() reads from VERSION file directly - no sync needed"

exit 0