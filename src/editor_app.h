#pragma once

#include <string>
#include <vector>
#include <memory>
#include <array>

struct GLFWwindow;
class TextEditor;

// ============================================================================
// Tab — represents one open file
// ============================================================================
struct EditorTab {
    std::string file_path;       // Empty = untitled
    std::string display_name;    // "untitled" or filename
    bool dirty = false;          // Has unsaved changes
    TextEditor* editor = nullptr; // Owned by tab
    int zoom_pct = 100;          // Zoom percentage
    
    // Bookmark and change tracking
    std::vector<int> bookmarks;  // Line numbers with bookmarks
    std::vector<int> changed_lines;  // Lines with changes (for change history)
    int last_saved_line_count = 0;  // Line count when last saved
    
    // Code folding
    std::vector<std::pair<int, int>> folds;  // (start_line, end_line) pairs
    
    // Per-tab state
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
};

struct ChangeHistoryEntry {
    int line;
    int type;  // 0=added, 1=modified, 2=deleted
    std::string timestamp;
};

// ============================================================================
// Settings — persisted to JSON beside executable
// ============================================================================
struct AppSettings {
    int window_w = 1280;
    int window_h = 800;
    bool dark_theme = true;
    bool show_status_bar = true;
    bool word_wrap = false;
    bool show_line_numbers = true;
    bool show_spaces = false;
    int tab_size = 4;          // Tab size in spaces
    int font_size = 16;
    std::string font_name = ""; // Font family name (empty = default)
    std::vector<std::string> recent_files;  // Last 10
    std::string last_open_dir;
};

// ============================================================================
// EditorApp — Main application
// ============================================================================
class EditorApp {
public:
EditorApp(int argc = 0, char* argv[] = nullptr);
    ~EditorApp();

    int run();
    void load_files_from_args(int argc, char* argv[]);

private:
    // Lifecycle
    void init();
    void shutdown();
    void load_settings();
    void save_settings();

    // Rendering
    void render();
    void render_menu_bar();
    void render_menu_file();
    void render_menu_edit();
    void render_menu_view();
    void render_menu_view_zoom();
    void render_menu_recent();
    void render_editor_area();
    void render_status_bar();
    void render_find_dialog();
    void render_replace_dialog();
    void render_goto_dialog();
    void render_command_palette();
    void render_font_dialog();
    void render_spaces_dialog();

    // File operations
    void new_tab();
    void new_window();
    void open_file(const std::string& path = "");
    bool save_tab(int idx);
    bool save_tab_as(int idx);
    void save_all();
    void close_tab(int idx);
    void close_window();

    // Search
    void find_next();
    void find_prev();
    void replace_one();
    void replace_all();

    // View
    void zoom_in();
    void zoom_out();
    void zoom_reset();
    void toggle_status_bar();
    void toggle_word_wrap();
    void toggle_line_numbers();
    void toggle_spaces();
    void toggle_theme();
    void set_tab_size(int size);
    void load_fonts();
    void rebuild_fonts();

    // Bookmarks
    void toggle_bookmark(int line);
    void next_bookmark();
    void prev_bookmark();
    void clear_bookmarks();

    // Change history
    void update_change_history();
    void clear_change_history();

    // Code folding
    void toggle_fold(int line);
    void fold_all();
    void unfold_all();

    // Gutter rendering
    void render_gutter(int tab_idx, float width);
    void render_editor_with_margins();

    // Helpers
    void apply_theme(bool dark);
    void apply_zoom(int tab_idx);
    std::string get_file_encoding(const std::string& path);
    std::string get_line_ending(const std::string& content);
    size_t get_file_size(const std::string& path);
    void add_recent_file(const std::string& path);
    bool confirm_discard_unsaved(int tab_idx);
    std::string get_temp_dir();

    // State
    GLFWwindow* window_ = nullptr;
    bool running_ = true;

    std::vector<EditorTab> tabs_;
    int active_tab_ = 0;

    AppSettings settings_;

    // Dialogs
    bool show_find_ = false;
    bool show_replace_ = false;
    bool show_goto_ = false;
    bool show_font_ = false;
    bool show_cmd_palette_ = false;
    bool show_spaces_ = false;

    // Find state
    char find_text_[256] = {0};
    char replace_text_[256] = {0};
    bool find_match_case_ = false;
    bool find_whole_word_ = false;
    bool find_wrap_ = true;

    // Goto state
    char goto_line_buf_[32] = {0};

    // Font state
    int font_size_temp_ = 16;
    std::string font_name_temp_ = "";

    // Command line args
    int argc_ = 0;
    char** argv_ = nullptr;

    // Spaces submenu state
    bool show_spaces_dialog_ = false;
    int tab_size_temp_ = 4;

    // Command palette
    char cmd_buf_[256] = {0};
};
