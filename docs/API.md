# pcode-editor API Reference

## Core Classes

### EditorApp (main application)

```cpp
class EditorApp {
public:
    EditorApp(int argc, char* argv[]);
    ~EditorApp();
    int run();                          // Main loop
    
    // Lifecycle
    void init();                        // Initialize
    void shutdown();                     // Cleanup
    void render();                       // Main render
    
    // Settings
    AppSettings settings_;               // Application settings
    
    // State
    int active_tab_ = 0;               // Current tab index
    std::vector<EditorTab> tabs_;        // Open tabs
    
    // Vim state
    VimMode vim_mode_ = VimMode::Normal;
    std::string vim_key_buffer_;
    int vim_count_ = 0;
};
```

### EditorTab (one open file)

```cpp
struct EditorTab {
    std::string file_path;       // File path
    std::string display_name;    // Display name
    bool dirty = false;        // Modified?
    TextEditor* editor = nullptr;
    
    // Split support
    std::vector<Split> splits_;
    int active_split = 0;
};
```

### Split (split view)

```cpp
struct Split {
    TextEditor* editor = nullptr;
    bool is_active = true;
    bool is_horizontal = true;
    float ratio = 0.5f;
};
```

### AppSettings (configuration)

```cpp
struct AppSettings {
    int window_w = 1280;
    int window_h = 800;
    bool dark_theme = true;
    bool enable_vim_mode = true;
    bool show_status_bar = false;
    bool show_tabs = false;
    bool show_line_numbers = false;
    bool show_minimap = false;
    bool show_code_folding = true;
    int tab_size = 4;
    int font_size = 18;
};
```

## Enums

### VimMode
```cpp
enum class VimMode {
    Normal,           // Navigation
    Insert,           // Text insertion
    OperatorPending,  // Waiting for motion
    Command           // Command line (:)
};
```

## Key Member Functions

### File Operations
```cpp
void new_tab();                     // Create new tab
void open_file(std::string path);   // Open file
void save_tab(int idx);             // Save file
void close_tab(int idx);            // Close tab
```

### View Operations
```cpp
void toggle_explorer();    // Show/hide file tree
void toggle_terminal();   // Show/hide terminal
void toggle_status_bar(); // Show/hide status bar
void toggle_word_wrap();  // Toggle word wrap
void toggle_line_numbers(); // Toggle line numbers
void toggle_minimap();   // Toggle minimap
void toggle_code_folding(); // Toggle folding

void split_horizontal();  // H-split current tab
void split_vertical();   // V-split current tab
void next_split();        // Focus next split
void prev_split();       // Focus prev split
```

### Menu Rendering
```cpp
void render_menu_bar();     // Main menu bar
void render_menu_file();   // File menu
void render_menu_edit();   // Edit menu
void render_menu_view();   // View menu
void render_menu_split();  // Split menu
void render_menu_help();   // Help menu
```

### Editor Rendering
```cpp
void render_sidebar();         // File tree + Git + Symbol
void render_editor_area();     // Tabs + splits
void render_splits(int idx);   // Render splits for tab
void render_status_bar();       // Bottom status bar
```

## Settings Functions

### Loading/Saving
```cpp
void load_settings();   // Load from settings.json
void save_settings();  // Save to settings.json
```

## Global State Variables

```cpp
GLFWwindow* window_;                       // GLFW window
bool show_file_tree_ = false;           // Explorer visible
bool show_terminal_ = false;             // Terminal visible
bool show_git_changes_ = false;         // Git panel visible
bool show_symbol_outline_ = false;       // Symbol outline visible
bool skip_texteditor_input_ = false;   // Vim input override
```

## Hooks and Callbacks

### GLFW Callbacks
- `glfwSetDropCallback()` - File drag-and-drop

### ImGui Callbacks
- Menu item clicks → Functions listed above
- Key presses → Vim handler (`handle_vim_key()`)