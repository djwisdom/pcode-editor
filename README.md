# pcode-editor

Personal Code Editor — A lightweight, cross-platform GUI text editor for programmers.

Built with **Dear ImGui** + **GLFW** + **ImGuiColorTextEdit** — one codebase, runs everywhere.

## Supported Platforms

| Platform | Backend | Status |
|----------|---------|--------|
| **Windows** | Win32 | ✅ Supported |
| **Linux** | Wayland | ✅ Supported |
| **FreeBSD** | X11 | ✅ Supported |
| **OpenBSD** | X11 | ✅ Supported |
| **NetBSD** | X11 | ✅ Supported |
| **macOS** | Cocoa | ✅ Supported |

## Features

- Syntax highlighting for 20+ languages (C, C++, Python, JavaScript, etc.)
- Line numbers and code folding
- Dockable UI panels (file tree, output, terminal)
- Command palette (Ctrl+P)
- Keyboard-first navigation
- Dark/light theme support
- Native file dialogs on all platforms
- <1MB binary, zero runtime dependencies

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

## Detailed Build Instructions

### FreeBSD / OpenBSD / NetBSD (X11)

**Install dependencies:**
```bash
# FreeBSD
sudo pkg install cmake xorg-libX11 xorgproto gtk3 mesa-libs pkgconf

# OpenBSD
sudo pkg_add cmake xorg-libraries gtk+3 mesa-drivers

# NetBSD
sudo pkgin install cmake xorg-libraries gtk3 mesa
```

**Build:**
```bash
mkdir -p build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DGLFW_BUILD_WAYLAND=OFF \
  -DGLFW_BUILD_X11=ON
make -j$(sysctl -n hw.ncpu)
./pcode-editor
```

**Or use the build script:**
```bash
./scripts/build-freebsd.sh Release
```

### Linux (Wayland)

**Install dependencies:**
```bash
# Ubuntu/Debian
sudo apt install cmake build-essential libwayland-dev wayland-protocols \
  libxkbcommon-dev libgtk-3-dev libgl1-mesa-dev pkg-config

# Fedora
sudo dnf install cmake gcc-c++ wayland-devel libxkbcommon-devel \
  gtk3-devel mesa-libGL-devel pkg-config

# Arch Linux
sudo pacman -S cmake base-devel wayland libxkbcommon gtk3 mesa
```

**Build:**
```bash
mkdir -p build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DGLFW_BUILD_WAYLAND=ON \
  -DGLFW_BUILD_X11=OFF
make -j$(nproc)
./pcode-editor
```

**Or use the build script:**
```bash
./scripts/build-linux.sh Release
```

### Windows (MSVC)

**Requirements:**
- Visual Studio 2022 (or Build Tools)
- CMake 3.16+

**Build (Developer Command Prompt):**
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

### Windows (MinGW-w64 / MSYS2)

**Install dependencies (MSYS2):**
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc make
```

**Build (PowerShell):**
```powershell
.\scripts\build-windows-mingw.ps1 Release
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

## Continuous Integration

This repository uses GitHub Actions to build and test all supported platforms on every push:

- **Linux (Wayland)** - Ubuntu 22.04
- **Windows (Win32)** - Windows Server 2022 with MSVC
- **FreeBSD (X11)** - FreeBSD 14.4

Build artifacts are automatically uploaded and can be downloaded from the Actions tab.

## Pre-commit Hook

A pre-commit hook is provided to validate builds before committing:

**Option 1: Install the hook**
```bash
cp .githooks/pre-commit .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

**Option 2: Configure git to use the hooks directory**
```bash
git config core.hooksPath .githooks
```

The hook will automatically build your code on commit and reject the commit if the build fails. Skip with `git commit --no-verify`.

## Dependencies

All dependencies are fetched automatically via CMake FetchContent:

| Library | Purpose | License |
|---------|---------|---------|
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate mode GUI | MIT |
| [GLFW](https://github.com/glfw/glfw) | Window/input management | zlib |
| [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) | Syntax highlighting, line numbers | MIT |
| [Native File Dialog](https://github.com/btzy/nativefiledialog-extended) | Cross-platform file dialogs | zlib |

## Architecture

```
src/
├── main.cpp          — Entry point
├── editor_app.h      — Application interface
├── editor_app.cpp    — Main application logic (UI, file I/O, commands)

scripts/
├── build.sh          — Universal build script
├── build-freebsd.sh  — BSD (X11) build script
├── build-linux.sh    — Linux (Wayland) build script
├── build-windows.bat — Windows (MSVC) build script
└── build-windows-mingw.ps1 — Windows (MinGW) build script

.githooks/
└── pre-commit        — Pre-commit build validation hook

.github/workflows/
└── build.yml         — CI/CD for all platforms

CMakeLists.txt        — Cross-platform build configuration
```

## Design Principles

1. **Keyboard-first** — every action has a keybinding
2. **Fast** — immediate mode rendering, <1ms frame time
3. **Minimal** — no bloat, no telemetry, no auto-updates
4. **Portable** — one codebase, Windows + Linux + BSD + macOS

## Troubleshooting

### FreeBSD: "library not found" errors
Ensure `/usr/local/lib` is in your library path:
```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### Linux: Wayland not working
Make sure you're running on a Wayland session. You can fall back to X11:
```bash
cmake .. -DGLFW_BUILD_WAYLAND=OFF -DGLFW_BUILD_X11=ON
```

### Windows: CMake generator errors
Specify the correct Visual Studio version:
```cmd
cmake .. -G "Visual Studio 17 2022" -A x64
```

## License

MIT — see LICENSE file.
