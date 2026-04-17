#!/usr/bin/env pwsh
# ============================================================================
# Sync version: Reads VERSION file and updates version string in code
# ============================================================================

$VERSION_FILE = "VERSION"
$CODE_FILE = "src/editor_app.cpp"
$VERSION_PATTERN = 'return "pCode Editor version \d+\.\d+\.\d+"'

# Read version from VERSION file
if (!(Test-Path $VERSION_FILE)) {
    Write-Host "❌ VERSION file not found"
    exit 1
}
$version = Get-Content $VERSION_FILE -Raw | ForEach-Object { $_.Trim() }
Write-Host "📌 Version from VERSION file: $version"

# Read current code version
$codeContent = Get-Content $CODE_FILE -Raw
if ($codeContent -match $VERSION_PATTERN) {
    $oldVersion = $Matches[0] -replace 'return "', '' -replace '"', ''
    Write-Host "📌 Current version in code: $oldVersion"
    
    if ($oldVersion -eq $version) {
        Write-Host "✅ Version already in sync"
        exit 0
    }
    
    # Update version in code
    $newPattern = 'return "pCode Editor version ' + $version + '"'
    $codeContent = $codeContent -replace $VERSION_PATTERN, $newPattern
    Set-Content -Path $CODE_FILE -Value $codeContent -NoNewline
    Write-Host "✅ Updated version to $version in $CODE_FILE"
} else {
    Write-Host "❌ Could not find version pattern in $CODE_FILE"
    exit 1
}

exit 0