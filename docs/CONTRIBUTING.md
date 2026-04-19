# Contributing to pcode-editor

## Getting Started

### Prerequisites
- C++17 compiler
- CMake 3.16+
- For Windows: Visual Studio 2022 or MSYS2/MinGW
- For Linux: Wayland development files
- For FreeBSD: X11 and GTK3

### Building
```bash
# Windows
./scripts/build-windows.bat

# Linux
./scripts/build-linux.sh

# FreeBSD
./scripts/build-freebsd.sh
```

### With vcpkg
```bash
# Install dependencies
vcpkg install glfw3 imgui[core,opengl3-glad] native-file-dialog-extended

# Build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## Code Style

- C++17 standard
- 4-space indentation
- No tabs
- Braces on same line: `if (condition) {`
- Opening brace on new line for class/struct definitions

## Key Functions

### File Operations
- `new_tab()` - Create new tab
- `open_file(path)` - Open file
- `save_tab(idx)` - Save tab
- `close_tab(idx)` - Close tab

### View Operations
- `toggle_explorer()` - Toggle file tree
- `toggle_terminal()` - Toggle terminal
- `toggle_status_bar()` - Toggle status bar
- `split_horizontal()` - H-split
- `split_vertical()` - V-split

### Vim Operations
- `handle_vim_key(key, count)` - Process vim key
- `apply_vim_motion(motion)` - Apply motion to cursor

## Testing

```bash
# Run tests
./build/Release/test_view_features.exe
./build/Release/test_vim_motions.exe
```

## Submitting Changes

1. Fork the repository
2. Create a feature branch
3. Make changes
4. Build and test
5. Update VERSION file (followsemsver)
6. Commit with descriptive message
7. Push and create PR

## Versioning

The project uses semantic versioning (MAJOR.MINOR.PATCH):
- MAJOR: Breaking changes
- MINOR: New features
- PATCH: Bug fixes

VERSION file format: `MAJOR.MINOR.PATCH (commit-hash)`

Every commit must bump the version.