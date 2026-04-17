# pcode-editor Developer Guide

A guide for developers who want to contribute to or extend pcode-editor.

## Version

This guide covers **pcode-editor version 0.2.28**.

---

## Architecture Overview

pcode-editor is built with a layered architecture:

```
.====================================================================.
|                        LAYER ARCHITECTURE                      |
+====================================================================+
|                                                                 |
|  +-----------------------------------------------------------+  |
|  |                    EDITOR APP CORE                         |  |
|  |              (editor_app.cpp main loop)                    |  |
|  +----------------------+------------------------------------+  |
|  |      TEXT EDITOR      |         UI RENDERING               |  |
|  |   (ImGuiColorText   |      (Dear ImGui widgets)          |  |
|  |    TextEdit)        |                                 |  |
|  +----------------------+------------------------------------+  |
|  +-----------------------------------------------------------+  |
|  |                 INPUT & WINDOW MANAGEMENT                  |  |
|  |                      (GLFW)                             |  |
|  +-----------------------------------------------------------+  |
|  |                 PLATFORM BACKENDS                      |  |
|  |      Win32  |  Wayland  |  X11  |  Cocoa               |  |
|  +----------------------+------------------------------------+  |
'===================================================================='
```

### Source Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Application entry point |
| `src/editor_app.h` | Class definitions and interfaces |
| `src/editor_app.cpp` | Main implementation (UI, commands, state) |

---

## Key Components

### EditorApp Class

```cpp
class EditorApp {
public:
    EditorApp(int argc, char* argv[]);
    ~EditorApp();
    int run();
    
    // Window management
    void init();
    void shutdown();
    void render();
    
    // File operations
    void new_tab();
    void open_file(const std::string& path);
    void save_file();
    void close_tab(int idx);
    
    // View
    void zoom_in();
    void zoom_out();
    void zoom_reset();
    void toggle_theme();
    void toggle_minimap();
    
    // Settings
    void load_settings();
    void save_settings();
};
```

### Tab Structure

Each tab contains:

```cpp
struct Tab {
    std::string file_path;
    std::string display_name;
    bool dirty;
    TextEditor* editor;
    int zoom_pct;
    std::vector<int> bookmarks;
    std::vector<int> changed_lines;
    std::vector<Split*> splits;
};
```

### Settings Structure

```cpp
struct AppSettings {
    int window_w = 1280;
    int window_h = 800;
    bool dark_theme = true;
    int font_size = 18;
    std::string font_name;
    std::vector<std::string> recent_files;
    // ... more settings
};
```

---

## Building

### Prerequisites

| Platform | Requirements |
|----------|--------------|
| Windows | Visual Studio 2022, CMake 3.16+ |
| Linux | GCC/Clang, CMake, Wayland dev libraries |
| macOS | Xcode, CMake |
| FreeBSD | GCC, CMake, X11 dev libraries |

### Build Commands

```bash
# Windows (MSVC)
scripts\build-windows.bat Release

# Linux
./scripts/build-linux.sh Release

# macOS
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Adding Features

### Adding a New Command

1. **Declare in header** (`editor_app.h`):
```cpp
void my_new_command();
```

2. **Implement** (`editor_app.cpp`):
```cpp
void EditorApp::my_new_command() {
    // Your implementation
}
```

3. **Add keybinding** (in `handle_input()` or menu):
```cpp
if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
    my_new_command();
}
```

### Adding a New Menu Item

Find the appropriate menu function (e.g., `render_menu_edit()`) and add:

```cpp
if (ImGui::MenuItem("My Command", "Ctrl+Shift+M")) {
    my_new_command();
}
```

### Adding a Vim Command

Add to `execute_vim_command()`:

```cpp
if (cmd == "mycommand") {
    my_new_command();
    return true;
}
```

---

## Code Style

### Formatting

- Indent: 4 spaces (no tabs)
- Max line length: 120 characters
- Brace style: K&R

### Naming Conventions

| Type | Convention | Example |
|------|-----------|---------|
| Classes | PascalCase | `EditorApp` |
| Methods | PascalCase | `open_file` |
| Variables | snake_case | `active_tab_` |
| Constants | UPPER_SNAKE | `MAX_RECENT_FILES` |
| Member vars | trailing `_` | `window_` |

### Comment Style

```cpp
// Section comment (uppercase)
// --------------------
// Function description
// ...
void function();
// 
// - param: description
// - returns: description
```

---

## Testing

### Running Tests

```bash
# Windows
.\build\Release\test_view_features.exe

# Linux
./build/test_view_features
```

### Writing Tests

Tests are in `tests/`:

```cpp
void test_feature() {
    // Arrange
    int result;
    
    // Act
    result = calculate();
    
    // Assert
    assert(result == expected);
}
```

---

## Debugging

### Windows

Open the solution in Visual Studio:
```cmd
devenv pcode-editor.sln
```

Set breakpoints in `editor_app.cpp`.

### Linux

```bash
gdb ./build/pcode-editor
# or
lldb ./build/pcode-editor
```

### Logging

The editor writes logs to stderr. Redirect to capture:

```bash
./pcode-editor 2>&1 | tee editor.log
```

---

## Dependencies

### FetchContent

Dependencies are automatically downloaded by CMake:

```cmake
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.90
)
```

### Updating Dependencies

Edit `CMakeLists.txt` to change versions:

```cmake
GIT_TAG v1.91  # Update this
```

---

## Project Structure

```
.====================================================================.
|                       PROJECT STRUCTURE                      |
+====================================================================+
|                                                                 |
|  pcode-editor/                                                 |
|  |                                                            |
|  +--- src/                                                    |
|  |   +-- main.cpp           -- Entry point                    |
|  |   +-- editor_app.h       -- Class definitions              |
|  |   +-- editor_app.cpp     -- Main implementation            |
|  |                                                            |
|  +--- tests/                                                 |
|  |   +-- test_view_features.cpp                             |
|  |                                                            |
|  +--- scripts/                                               |
|  |   +-- build.sh              -- Universal build             |
|  |   +-- build-linux.sh       -- Linux (Wayland)             |
|  |   +-- build-windows.bat   -- Windows (MSVC)                 |
|  |   +-- build-freebsd.sh    -- BSD (X11)                  |
|  |                                                            |
|  +--- .githooks/                                            |
|  |   +-- pre-commit         -- Build validation               |
|  |                                                            |
|  +--- docs/                                                  |
|  |   +-- userguide.md                                       |
|  |   +-- developer.md                                      |
|  |   +-- faq.md                                           |
|  |                                                            |
|  +--- CMakeLists.txt                                        |
|  +--- pcode-settings.json                                  |
|  +--- VERSION                                             |
|  +--- LICENSE                                             |
|  +--- README.md                                            |
'===================================================================='
```

---

## Common Tasks

### Adding a New Setting

1. **Add to AppSettings struct** (`editor_app.h`):
```cpp
struct AppSettings {
    // ... existing
    bool my_new_setting = false;
};
```

2. **Load/Save** (`editor_app.cpp`):
```cpp
// In load_settings()
s.my_new_setting = get_bool("my_new_setting", false);

// In save_settings()
f << "  \"my_new_setting\": " << (s.my_new_setting ? "true" : "false") << ",\n";
```

### Adding UI Elements

```cpp
// In render() function
if (ImGui::Begin("My Panel", nullptr, flags)) {
    if (ImGui::Button("Click Me")) {
        // Handle click
    }
    ImGui::End();
}
```

### Terminal Integration

```cpp
void EditorApp::start_terminal() {
#ifdef _WIN32
    // Use CreateProcess with pipes
#else
    // Use forkpty
#endif
}
```

---

## Contribution Guidelines

1. **Fork** the repository
2. **Create** a feature branch
3. **Make** your changes
4. **Test** the build
5. **Commit** with clear messages
6. **Push** and create PR

### Commit Messages

```
type: short description

Longer description if needed.

- bullet points of changes
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`

---

## License

BSD 2-Clause License - see LICENSE file.