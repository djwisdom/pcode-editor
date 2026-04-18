# ============================================================================
# Portable Installer for pCode Editor
# Run this in user context - no admin required
# ============================================================================

param(
    [string]$InstallDir = "$env:USERPROFILE\Apps\pcode-editor"
)

$ErrorActionPreference = "Stop"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  pCode Editor - Portable Installer" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Create install directory
if (-not (Test-Path $InstallDir)) {
    Write-Host "[+] Creating install directory..." -ForegroundColor Green
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# Copy files
Write-Host "[+] Copying files..." -ForegroundColor Green
$SourceDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Copy-Item "$SourceDir\bin\*" -Destination $InstallDir -Recurse -Force

# Create Start Menu shortcut
Write-Host "[+] Creating Start Menu shortcut..." -ForegroundColor Green
$ShortcutPath = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\pCode Editor.lnk"
$WScriptShell = New-Object -ComObject WScript.Shell
$Shortcut = $WScriptShell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = "$InstallDir\pcode-editor.exe"
$Shortcut.WorkingDirectory = $InstallDir
$Shortcut.Description = "pCode Editor - Vim-like code editor"
$Shortcut.Save()

# Create Desktop shortcut (optional)
if (Test-Path "$env:USERPROFILE\Desktop") {
    $DesktopShortcut = "$env:USERPROFILE\Desktop\pCode Editor.lnk"
    $Shortcut = $WScriptShell.CreateShortcut($DesktopShortcut)
    $Shortcut.TargetPath = "$InstallDir\pcode-editor.exe"
    $Shortcut.WorkingDirectory = $InstallDir
    $Shortcut.Description = "pCode Editor - Vim-like code editor"
    $Shortcut.Save()
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Installation complete!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Install location: $InstallDir" -ForegroundColor White
Write-Host ""
Write-Host "  Run: $InstallDir\pcode-editor.exe" -ForegroundColor White
Write-Host "  Or from Start Menu" -ForegroundColor White
Write-Host ""