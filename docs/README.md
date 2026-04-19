# pcode-editor

A Vim-like code editor using Dear ImGui, GLFW, and ImGuiColorTextEdit.

## Current Capabilities

- **Code editing** - Syntax highlighting via TextEditor (ImGuiColorTextEdit)
- **Multi-tab editing** - Multiple files in tabs
- **File operations** - Open, save, new, recent files
- **Basic edit operations** - Copy, cut, paste, undo, redo
- **Find/Replace** - Search and replace text
- **Status bar** - Shows file info, cursor position
- **Dark/Light theme** - Toggle via menu
- **Font resizing** - Zoom in/out
- **Multi-viewport** - ImGui viewports enabled

## Removed Features (Not Working)

- Terminal (removed)
- Explorer/File tree (removed)
- Vim mode (removed)
- Bookmarks (removed)
- Minimap (removed)
- Code folding (removed)
- Line highlight (removed)
- Change history (removed)

## Building

```bash
./scripts/build-windows.bat   # Windows
./scripts/build-linux.sh  # Linux
```

## Keyboard Shortcuts

- `Ctrl+O` - Open file
- `Ctrl+S` - Save
- `Ctrl+N` - New file
- `Ctrl+F` - Find
- `Ctrl+H` - Replace
- `Ctrl+W` - Close tab
- `Ctrl++` - Zoom in
- `Ctrl+-` - Zoom out
- `Ctrl+0` - Reset zoom

## Version

0.7.1