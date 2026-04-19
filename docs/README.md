# pcode-editor

A code editor using Dear ImGui, GLFW, and ImGuiColorTextEdit.

## Current Capabilities

- **Code editing** - Syntax highlighting via TextEditor
- **Multi-tab editing** - Multiple files in tabs with collapsible Poorman's tree-sitter
- **File operations** - Open, save, new, recent files
- **Basic edit operations** - Copy, cut, paste, undo, redo
- **Find/Replace** - Search and replace text
- **Status bar** - Shows mode, filename, cursor position, encoding, tab size
- **Command bar** - Bottom input for commands
- **Dark/Light theme** - Toggle via menu
- **Font resizing** - Zoom in/out

## Current Layout

```
Menu [=] Tab[+] Tab[+] Tab[+]  (hamburger menu contains File/Edit/View/Help)
[|< ][        editor area                            ]
[ ..][                                            ]
[file][        editor area                            ]
[file][                                            ]
[file][        editor area                            ]
[ NORMAL untitled * Ln 1, Col 1 UTF-8 LF Tab:4 ]  (status bar)
[:command_bar_____________________________________]  (command bar)
```

## Removed Features (Not Working)

- Terminal - ImGui can't get raw keyboard input for interactive shells
- Vim mode - Incompatible with ImGui's input model
- Minimap - Not implemented
- Code folding - Not implemented
- Bookmarks - Not implemented
- Multi-viewport - Disabled (windows stay in main window)

## Building

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

0.8.0