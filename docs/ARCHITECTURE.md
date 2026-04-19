# pcode-editor Architecture

## Overview

pcode-editor is a Vim-like code editor built with **Dear ImGui**, **GLFW**, and **ImGuiColorTextEdit**.

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Application Layer                          │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              EditorApp (main.cpp)                    │    │
│  │  - Window/GLFW management                          │    │
│  │  - Main render loop                                │    │
│  │  - File dialogs (NFD)                              │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      UI Layer (editor_app.cpp)                    │
│  ┌──────────────┬──────────────┬──────────────┬──────────────┐  │
│  │ Menu Bar     │ Sidebar      │ Editor      │ Status Bar   │  │
│  │ File/Edit/  │ Explorer    │ Tabs +      │ Mode info    │  │
│  │ View/Split  │ Git/Symbol  │ Splits      │ File info   │  │
│  └──────────────┴──────────────┴──────────────┴──────────────┘  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ Dialogs: Find, Replace, Goto, Font, About, Command Palette │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   Rendering Layer (ImGui)                     │
│  ┌─────────────────────┐  ┌─────────────────────┐             │
│  │ ImGui + ImGui_Impl  │  │ ImGuiColorTextEdit  │             │
│  │ (menus, dialogs)    │  │ (code editing)     │             │
│  └─────────────────────┘  └─────────────────────┘             │
└──────────────────────────────────��──────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Platform Layer (GLFW)                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐   │
│  │ Windows  │  │ Linux   │  │ FreeBSD │  │    macOS         │   │
│  │ (Win32)  │  │(Wayland) │  │  (X11)  │  │    (Cocoa)      │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

## Data Structures

### EditorTab
```cpp
struct EditorTab {
    std::string file_path;       // Empty = untitled
    std::string display_name;    // "untitled" or filename
    bool dirty = false;        // Has unsaved changes
    TextEditor* editor = nullptr;
    std::vector<int> bookmarks;
    std::vector<int> changed_lines;
};
```

### Split
```cpp
struct Split {
    TextEditor* editor = nullptr;
    bool is_active = true;
    bool is_horizontal = true;
    float ratio = 0.5f;
};
```

### EditorApp Settings
```cpp
struct AppSettings {
    int window_w = 1280, window_h = 800;
    bool dark_theme = true;
    bool enable_vim_mode = true;
    bool show_status_bar = false;
    bool show_tabs = false;
    int tab_size = 4;
    int font_size = 18;
};
```

### Vim Modes
```cpp
enum class VimMode {
    Normal,     // Navigation mode (default)
    Insert,     // Insert text
    OperatorPending,  // Waiting for motion after operator
    Command     // : command line
};
```

## Key Files

| File | Lines | Purpose |
|------|-------|----------|
| `src/main.cpp` | ~280 | Entry point, initialization |
| `src/editor_app.h` | ~356 | Class definitions |
| `src/editor_app.cpp` | ~4200 | Main implementation |

## Build System

```
pcode-editor/
├── CMakeLists.txt          # CMake build (vcpkg-compatible)
├── vcpkg.json              # vcpkg manifest
├── scripts/
│   ├── build-windows.bat  # Windows
│   ├── build-linux.sh     # Linux
│   └── build-freebsd.sh  # FreeBSD
└── docs/
    ├── ARCHITECTURE.md    # This file
    ├── developer.md      # Developer guide
    └── userguide.md     # User documentation
```

## Dependencies

| Library | Version | Purpose |
|----------|---------|---------|
| Dear ImGui | docking | GUI framework |
| GLFW | 3.4 | Window/input |
| ImGuiColorTextEdit | master | Code editor |
| native-file-dialog-extended | | File dialogs |

## Rendering Flow

```
glfwPollEvents()
    │
    ▼
ImGui_ImplOpenGL3_NewFrame()
ImGui::NewFrame()
    │
    ▼
render_menu_bar()     → File, Edit, View, Split, Help menus
    │
    ▼
render_sidebar()     → Explorer, Git, Symbol panels
    │
    ▼
render_editor_area() → Tabs + Splits + TextEditor
    │
    ▼
render_status_bar()   → Bottom status bar
    │
    ▼
render_dialogs()    → Find, Replace, Goto, etc.
    │
    ▼
ImGui::Render()
ImGui_ImplOpenGL3_RenderDrawData()
glfwSwapBuffers()
```

## State Management

- **AppSettings**: Persisted to `~/.config/pcode-editor/settings.json`
- **Session**: Tab state saved on exit, restored on start
- **VimMode**: Global state (`vim_mode_`), affects key handling

## Event Flow

```
Key Pressed
    │
    ├── Vim Mode Enabled?
    │   ├── Yes → Check Escape → Normal mode
    │   │       → Check i/a/o → Insert mode
    │   │       → Handle motion (h,j,k,l,w,$,etc.)
    │   │
    │   └── No → Let ImGui pass to TextEditor
    │
    ▼
TextEditor receives key input
```