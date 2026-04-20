# Cross-Platform Build System

This document describes the cross-platform build system for pcode-editor.

## Supported Platforms

| Platform | Backend | CI Runner | Build Script |
|----------|---------|-----------|--------------|
| Windows (Win32) | Win32 API + OpenGL | windows-latest (MSVC) | `scripts/build-windows.bat` |
| Linux | Wayland + GTK3 | ubuntu-22.04 | `scripts/build-linux.sh` |
| FreeBSD | X11 + GTK3 | FreeBSD 14.4 | `scripts/build-freebsd.sh` |
| OpenBSD | X11 + GTK3 | - | `scripts/build-freebsd.sh` |
| NetBSD | X11 + GTK3 | - | `scripts/build-freebsd.sh` |
| macOS | Cocoa + OpenGL | - | Manual build |

## Architecture

### CMakeLists.txt
The main CMake configuration file handles:
- Platform detection (Windows, Linux, BSD, macOS)
- GLFW configuration (Wayland for Linux, X11 for BSD)
- Platform-specific linking:
  - **Windows**: opengl32, gdi32
  - **Linux**: Wayland libraries, GTK3, OpenGL
  - **BSD**: X11, GTK3, OpenGL, `/usr/local/lib` path
  - **macOS**: Cocoa, OpenGL frameworks

### Build Scripts

#### Universal Build Script
- **File**: `scripts/build.sh`
- **Usage**: `./scripts/build.sh [Release|Debug]`
- Auto-detects platform and calls the appropriate platform-specific script

#### Platform-Specific Scripts
- **FreeBSD**: `scripts/build-freebsd.sh` - Configures X11 backend
- **Linux**: `scripts/build-linux.sh` - Configures Wayland backend
- **Windows (MSVC)**: `scripts/build-windows.bat` - Visual Studio build
- **Windows (MinGW)**: `scripts/build-windows-mingw.ps1` - MinGW-w64 build

### GitHub Actions CI/CD

**File**: `.github/workflows/build.yml`

Builds on every push to main/master and pull requests:
1. **Linux (Wayland)** - Ubuntu 22.04 with Wayland dependencies
2. **Windows (Win32)** - Windows Server with MSVC
3. **FreeBSD (X11)** - FreeBSD 14.4 via cross-platform-actions action

Each job:
- Installs platform-specific dependencies
- Configures CMake with correct backend flags
- Builds with parallel compilation
- Verifies executable exists
- Uploads artifact for download

### Pre-commit Hook

**File**: `.githooks/pre-commit`

Validates builds before allowing commits:
- Auto-detects platform
- Creates temporary build directory
- Attempts full build
- Rejects commit if build fails
- Cleanup after validation

**Installation**:
```bash
# Option 1: Copy to .git/hooks
cp .githooks/pre-commit .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit

# Option 2: Configure git to use .githooks directory
git config core.hooksPath .githooks
```

**Skip**: `git commit --no-verify`

## Platform-Specific Details

### Windows (Win32)
- **Backend**: Win32 API
- **Compiler**: MSVC or MinGW-w64
- **File Dialog**: IFileOpenDialog/IFileSaveDialog (Vista+)
- **Graphics**: OpenGL via WGL
- **Dependencies**: None (system libraries only)

### Linux (Wayland)
- **Backend**: Wayland
- **Compiler**: GCC or Clang
- **File Dialog**: GTK3 (native GTK file chooser)
- **Graphics**: OpenGL via GLX/EGL
- **Dependencies**:
  - libwayland-dev
  - wayland-protocols
  - libxkbcommon-dev
  - libgtk-3-dev
  - libgl1-mesa-dev

### BSD (X11)
- **Backend**: X11
- **Compiler**: Clang (FreeBSD) or GCC
- **File Dialog**: GTK3 (native GTK file chooser)
- **Graphics**: OpenGL via GLX
- **Dependencies**:
  - xorg-libX11
  - xorgproto
  - gtk3
  - mesa-libs
  - pkgconf
- **Notes**: Libraries in `/usr/local/lib`, requires `target_link_directories`

### macOS
- **Backend**: Cocoa
- **Compiler**: Clang (Xcode)
- **File Dialog**: NSOpenPanel/NSSavePanel
- **Graphics**: OpenGL framework
- **Dependencies**: None (system frameworks only)

## Build Matrix

| Platform | Window System | File Dialog | OpenGL Loader | Linker Flags |
|----------|--------------|-------------|---------------|--------------|
| Windows | Win32 | IFileDialog | GLAD | opengl32, gdi32 |
| Linux | Wayland | GTK3 | GLAD | wayland-*, gtk-3, GL |
| BSD | X11 | GTK3 | GLAD | X11, gtk-3, GL, -L/usr/local/lib |
| macOS | Cocoa | NSPanel | Native | -framework OpenGL, Cocoa |

## Troubleshooting

### Build fails on FreeBSD with "library not found"
The linker can't find `/usr/local/lib`. This is fixed in CMakeLists.txt with:
```cmake
target_link_directories(${PROJECT_NAME} PRIVATE /usr/local/lib)
```

### Linux build uses X11 instead of Wayland
Ensure CMake flags are set correctly:
```bash
cmake .. -DGLFW_BUILD_WAYLAND=ON -DGLFW_BUILD_X11=OFF
```

### Windows CMake generator not found
Specify the correct Visual Studio version:
```cmd
cmake .. -G "Visual Studio 17 2022" -A x64
```

### Wayland session not available on Linux
You can fall back to X11:
```bash
cmake .. -DGLFW_BUILD_WAYLAND=OFF -DGLFW_BUILD_X11=ON
```

## Artifacts

After each successful CI run, download build artifacts from:
1. GitHub repository → Actions tab → Latest workflow run
2. Click on the platform job (linux/windows/freebsd)
3. Download the artifact section at the bottom

Artifacts contain:
- **pcode-editor-linux**: ELF 64-bit executable
- **pcode-editor-windows**: PE32+ executable
- **pcode-editor-freebsd**: ELF 64-bit executable

## Future Enhancements

- [ ] Add macOS GitHub Actions runner
- [ ] Create release packages (zip, tar.gz, deb, rpm)
- [ ] Add code signing for Windows/macOS
- [ ] Implement auto-update mechanism
- [ ] Add installer generation (NSIS, pkg, deb, rpm)

