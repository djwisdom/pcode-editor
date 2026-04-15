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
    int font_size = 16;
    std::vector<std::string> recent_files;  // Last 10
    std::string last_open_dir;
};

// ============================================================================
// EditorApp — Main application
// ============================================================================
class EditorApp {
public:
    EditorApp();
    ~EditorApp();

    int run();

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
    void toggle_theme();

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

    // Command palette
    char cmd_buf_[256] = {0};
};
