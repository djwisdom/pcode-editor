# pcode-editor

Personal Code Editor — A lightweight, cross-platform Vim-like GUI text editor for programmers.

Built with **Dear ImGui** + **GLFW** + **ImGuiColorTextEdit** — one codebase, runs everywhere.

## Version

**pcode-editor v0.2.28** (2026-04-17)

---

## Supported Platforms

| Platform | Backend | Status |
|----------|---------|--------|
| **Windows** | Win32 | Supported |
| **Linux** | Wayland | Supported |
| **FreeBSD** | X11 | Supported |
| **OpenBSD** | X11 | Supported |
| **NetBSD** | X11 | Supported |
| **macOS** | Cocoa | Supported |

---

## Features

### Editing
- Syntax highlighting for 20+ languages (C, C++, Python, JavaScript, etc.)
- Line numbers with change history tracking
- Code folding
- Bookmarks
- Multiple tabs with split windows
- Find and replace
- Word wrap

### Vim Mode
- NORMAL mode for navigation and commands
- INSERT mode for text entry
- COMMAND mode for ex commands (`:w`, `:q`, `:sp`, etc.)
- Visual mode support

### Interface
- Split windows (`:sp` horizontal, `:vsp` vertical)
- Minimap for quick navigation
- Floating command palette (Ctrl+P)
- Sidebar with Files/Git/Symbols
- Status bar with mode, cursor position, encoding
- Gutter with line numbers and change indicators

### Terminal Integration
- Built-in terminal (`:term`, `:shell`, `:sh`)
- Direct keyboard input to shell
- Independent font zoom for terminal (Ctrl++/-)

### Settings
- Cross-platform settings file (`pcode-settings.json`)
- Font customization (size, family)
- Theme toggle (dark/light)
- Recent files management

### Keyboard Shortcuts
- Full keyboard-first navigation
- Vim-style keybindings
- Ctrl+W for split navigation
- Command palette

---

## Quick Start

### Universal Build Script

```bash
# Auto-detects platform and builds accordingly
./scripts/build.sh Release
```

### Platform-Specific Build Scripts

```bash
# FreeBSD/OpenBSD/NetBSD (X11)
./scripts/build-freebsd.sh Release

# Linux (Wayland)
./scripts/build-linux.sh Release

# Windows (MSVC)
scripts\build-windows.bat Release

# Windows (MinGW/MSYS2 PowerShell)
.\scripts\build-windows-mingw.ps1 Release
```

### Running the Editor

```bash
# Windows
.\build\Release\pcode-editor.exe

# Linux/macOS/BSD
./build/Release/pcode-editor

# Open a file
pcode-editor filename.cpp
```

---

## Detailed Build Instructions

### Windows (MSVC)

**Requirements:**
- Visual Studio 2022 (or Build Tools)
- CMake 3.16+

**Build:**
```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
.\Release\pcode-editor.exe
```

**Or use the build script:**
```cmd
scripts\build-windows.bat Release
```

### Linux (Wayland)

**Install dependencies:**
```bash
# Ubuntu/Debian
sudo apt install cmake build-essential libwayland-dev wayland-protocols \
  libxkbcommon-dev libgtk-3-dev libgl1-mesa-dev pkg-config
```

**Build:**
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DGLFW_BUILD_WAYLAND=ON
make -j$(nproc)
./pcode-editor
```

### FreeBSD / OpenBSD / NetBSD (X11)

**Install dependencies:**
```bash
# FreeBSD
sudo pkg install cmake xorg-libX11 xorgproto gtk3 mesa-libs pkgconf
```

**Build:**
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DGLFW_BUILD_X11=ON
make -j$(sysctl -n hw.ncpu)
./pcode-editor
```

### macOS

**Install dependencies:**
```bash
brew install cmake
```

**Build:**
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
./pcode-editor
```

---

## Command Reference

### File Commands

| Command | Action |
|---------|--------|
| `:w` | Save file |
| `:q` | Close tab |
| `:q!` | Close without saving |
| `:wq` | Save and close |
| `:e filename` | Open file |

### View Commands

| Command | Action |
|---------|--------|
| `:sp` | Horizontal split |
| `:vsp` | Vertical split |
| `:set nu` | Show line numbers |
| `:set nonu` | Hide line numbers |
| `:set wrap` | Enable word wrap |
| `:set nowrap` | Disable word wrap |

### Terminal Commands

| Command | Action |
|---------|--------|
| `:term` | Open terminal |
| `:shell` | Open default shell |
| `:sh` | Open default shell |

### Zoom Commands

| Command | Action |
|---------|--------|
| `Ctrl++` | Zoom in |
| `Ctrl+-` | Zoom out |
| `Ctrl+0` | Reset zoom |

---

## Keyboard Shortcuts

### File Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New tab |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save |
| `Ctrl+W` | Close tab |

### Edit Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+C` | Copy |
| `Ctrl+V` | Paste |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+F` | Find |

### View Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+P` | Command palette |
| `Ctrl+\`` | Toggle terminal |

### Split Navigation

| Shortcut | Action |
|----------|--------|
| `Ctrl+W` h | Focus left |
| `Ctrl+W` j | Focus down |
| `Ctrl+W` k | Focus up |
| `Ctrl+W` l | Focus right |
| `Ctrl+W` o | Close others |

---

## Settings

Settings are stored in `pcode-settings.json`:

```json
{
  "window_w": 1280,
  "window_h": 800,
  "dark_theme": true,
  "font_size": 18,
  "font_name": "JetBrains Mono",
  "tab_size": 4,
  "word_wrap": true,
  "show_line_numbers": true,
  "show_minimap": true
}
```

---

## Architecture

```
src/
├── main.cpp          — Entry point
├── editor_app.h      — Class definitions
└── editor_app.cpp    — Main implementation

scripts/
├── build.sh          — Universal build script
├── build-freebsd.sh  — BSD (X11)
├── build-linux.sh    — Linux (Wayland)
├── build-windows.bat — Windows (MSVC)
└── build-windows-mingw.ps1 — Windows (MinGW)

docs/
├── userguide.md      — User documentation
├── developer.md     — Developer guide
├── imgui_tutorial.md — Dear ImGui guide
└── faq.md           — FAQ

CMakeLists.txt        — Cross-platform build config
VERSION              — Version file
pcode-settings.json — User settings
```

---

## Dependencies

All dependencies are fetched automatically via CMake FetchContent:

| Library | Purpose | License |
|---------|---------|---------|
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate mode GUI | MIT |
| [GLFW](https://github.com/glfw/glfw) | Window/input management | zlib |
| [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) | Syntax highlighting | MIT |
| [Native File Dialog](https://github.com/btzy/nativefiledialog-extended) | Cross-platform file dialogs | zlib |

---

## Design Principles

1. **Keyboard-first** — every action has a keybinding
2. **Fast** — immediate mode rendering, minimal dependencies
3. **Minimal** — no bloat, no telemetry, no auto-updates
4. **Portable** — one codebase, Windows + Linux + BSD + macOS

---

## Troubleshooting

### Fonts too small on HDPI

Edit `pcode-settings.json`:
```json
"font_size": 18
```

### Terminal not responding

Click on the terminal window to focus it before typing.

### Build fails

1. Ensure all platform-specific dependencies are installed
2. Use the provided build scripts
3. See `docs/faq.md` for detailed solutions

---

## Documentation

- [User Guide](docs/userguide.md) — For end users
- [Developer Guide](docs/developer.md) — For contributors
- [Dear ImGui Tutorial](docs/imgui_tutorial.md) — ImGui learning resource
- [Versioning Strategy](docs/versioning.md) — Release and tagging process
- [FAQ](docs/faq.md) — Common issues and solutions

---

## License

BSD 2-Clause License — see LICENSE file.