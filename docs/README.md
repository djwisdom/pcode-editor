# pcode-editor Documentation

## Quick Links

| Document | Description |
|----------|-------------|
| [README](../README.md) | Project overview |
| [User Guide](userguide.md) | End-user documentation |
| [Developer's Guide](developer.md) | Contributing & architecture |
| [API Reference](API.md) | Class & function reference |
| [Architecture](ARCHITECTURE.md) | System design |
| [Contributing](CONTRIBUTING.md) | How to contribute |

## Building

### Windows
```bash
./scripts/build-windows.bat
```

### Linux
```bash
./scripts/build-linux.sh  # or ./scripts/build-linux.sh Release vcpkg
```

### FreeBSD
```bash
./scripts/build-freebsd.sh
```

### With vcpkg
```bash
vcpkg install glfw3 imgui[core,opengl3-glad] native-file-dialog-extended
cmake -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## Project Structure

```
pcode-editor/
├── src/
│   ├── main.cpp          # Entry point
│   ├── editor_app.h      # Class definitions
│   └── editor_app.cpp    # Implementation (~4200 lines)
├── tests/
│   ├── test_vim_motions.cpp
│   └── test_view_features.cpp
├── scripts/
│   ├── build-windows.bat
│   ├── build-linux.sh
│   └── build-freebsd.sh
├── docs/
│   ├── ARCHITECTURE.md
│   ├── API.md
│   ├── developer.md
│   ├── CONTRIBUTING.md
│   └── userguide.md
├── CMakeLists.txt
├── vcpkg.json
└── VERSION
```

## Key Features

- Vim-like keybindings (toggle in View → Vim Mode)
- Syntax highlighting (20+ languages)
- Split views (horizontal/vertical)
- File explorer sidebar
- Terminal support
- Tab system
- Drag-and-drop file opening

## Version

Current: 0.3.0

See [VERSION](../VERSION) for full version and commit hash.