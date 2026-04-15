# pcode-editor

Personal Code Editor — A lightweight, cross-platform GUI text editor for programmers.

Built with **Dear ImGui** + **GLFW** + **ImGuiColorTextEdit** — one codebase, runs on Windows and Linux.

## Features

- Syntax highlighting for 20+ languages (C, C++, Python, JavaScript, etc.)
- Line numbers and code folding
- Dockable UI panels (file tree, output, terminal)
- Command palette (Ctrl+P)
- Keyboard-first navigation
- Dark/light theme support
- <1MB binary, zero runtime dependencies

## Building

### Linux

```bash
sudo apt install cmake build-essential libgl1-mesa-dev libx11-dev
mkdir build && cd build
cmake ..
make -j$(nproc)
./code-editor
```

### Windows

```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
.\Release\code-editor.exe
```

## Dependencies

All dependencies are fetched automatically via CMake FetchContent:

| Library | Purpose | License |
|---------|---------|---------|
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate mode GUI | MIT |
| [GLFW](https://github.com/glfw/glfw) | Window/input management | zlib |
| [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) | Syntax highlighting, line numbers | MIT |

## Architecture

```
src/
├── main.cpp          — Entry point
├── editor_app.h      — Application interface
├── editor_app.cpp    — Main application logic (UI, file I/O, commands)
CMakeLists.txt        — Build configuration (FetchContent for deps)
```

## Design Principles

1. **Keyboard-first** — every action has a keybinding
2. **Fast** — immediate mode rendering, <1ms frame time
3. **Minimal** — no bloat, no telemetry, no auto-updates
4. **Portable** — one codebase, Windows + Linux

## License

MIT — see LICENSE file.
