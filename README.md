# pcode-editor

A lightweight code editor using Dear ImGui, GLFW, and ImGuiColorTextEdit.

## Version

**pcode-editor v0.8.0**

## Features

- **Syntax highlighting** - 20+ languages (C, C++, Python, JavaScript, etc.)
- **Multi-tab editing** - Multiple files in tabs
- **Status bar** - Shows file info, cursor position, encoding
- **Dark/Light theme** - Toggle via menu
- **Font resizing** - Zoom in/out
- **File/Symbol/Git views** - Resizable sidebar with symbol outline (Poorman's parser)

## Building

```bash
./scripts/build-windows.bat   # Windows
./scripts/build-linux.sh    # Linux
```

## Running

```bash
./build/Release/pcode-editor.exe  # Windows
./build/Release/pcode-editor    # Linux
```

## Keyboard Shortcuts

- `Ctrl+N` - New tab
- `Ctrl+O` - Open file
- `Ctrl+S` - Save
- `Ctrl+W` - Close tab
- `Ctrl++` - Zoom in
- `Ctrl+-` - Zoom out
- `Ctrl+0` - Reset zoom

## Removed Features

The following features were planned but not working and have been removed:

- Terminal - ImGui can't get raw keyboard input for interactive shells
- Vim mode - Incompatible with ImGui's input model
- Minimap - Not implemented
- Code folding - Not implemented  
- Bookmarks - Not implemented
- File explorer - Not implemented

## Settings

Settings stored in `pcode-settings.json`:

```json
{
  "window_w": 1280,
  "window_h": 800,
  "dark_theme": true,
  "font_size": 18,
  "font_name": "JetBrains Mono",
  "tab_size": 4
}
```

## Dependencies

All dependencies fetched automatically via CMake FetchContent:

- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI
- [GLFW](https://github.com/glfw/glfw) - Window/input management
- [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) - Syntax highlighting
- [Native File Dialog](https://github.com/btzy/nativefiledialog-extended) - File dialogs

## License

BSD 2-Clause License - see LICENSE file.