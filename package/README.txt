# pCode Editor - Portable Package

A Vim-like code editor built with Dear ImGui and GLFW.

## Installation (User Directory - No Admin Required)

1. Double-click `install.ps1` in Windows Explorer
   - Or run from PowerShell: `.\install.ps1`
2. Default install: `%USERPROFILE%\Apps\pcode-editor`
3. Custom location: `.\install.ps1 -InstallDir "C:\Your\Path"`

## Features

- Vim-like keybindings
- Syntax highlighting (ImGuiColorTextEdit)
- File explorer sidebar
- Split views (horizontal/vertical)
- Terminal support
- Tab system with keyboard shortcuts

## Shortcuts

- `Ctrl+N` - New tab
- `Ctrl+O` - Open file
- `Ctrl+S` - Save
- `Ctrl+B` - Toggle explorer
- `Ctrl+\`` - Toggle terminal
- `Ctrl+Tab` - Next tab
- `Ctrl+Shift+Tab` - Previous tab
- Split: View menu → Split Horizontal/Vertical

## System Requirements

- Windows 10+
- No additional dependencies (statically linked)

## Uninstall

1. Delete the install folder
2. Remove shortcuts from Start Menu and Desktop