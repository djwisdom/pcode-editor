#pragma once

// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 pCode Editor Development Team
// Author: Dennis O. Esternon <djwisdom@serenityos.org>
// Contributors: see CONTRIBUTORS or git history

#include <string>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>

#include "editor_notifications.h"

struct GLFWwindow;
class TextEditor;

// ============================================================================
// Split — represents a split view within a tab
// ============================================================================
struct Split {
    TextEditor* editor = nullptr;       // The editor for this split (can be shared)
    bool editor_owned = true;        // Should we delete editor on split close
    bool is_active = true;         // Is this split focused
    float ratio = 0.5f;           // Split ratio (0.0-1.0)
    bool is_horizontal = true;    // true = horizontal (top/bottom), false = vertical
    
    // Terminal instance for this split (independent terminals)
    bool has_terminal = false;
    void* terminal_process = nullptr;
    void* terminal_stdin = nullptr;
    void* terminal_stdout = nullptr;
    std::string terminal_output;
    std::vector<std::string> terminal_history;
    
#ifdef _WIN32
    // ConPTY handles for Windows terminal
    void* conpty_handle = nullptr;
#endif
};

// ============================================================================
// Tab — represents one open file
// ============================================================================
struct EditorTab {
    std::string file_path;       // Empty = untitled
    std::string display_name;    // "untitled" or filename
    bool dirty = false;          // Has unsaved changes
    bool first_render = true;    // Hasn't been rendered yet
    TextEditor* editor = nullptr; // Owned by tab
    bool is_terminal = false;    // This tab is a terminal
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
    
    // Splits
    std::vector<class Split*> splits;
    int active_split = 0;
    
    // Cached file info for status bar
    std::string file_encoding = "UTF-8";
    std::string line_ending = "LF";
    size_t file_size = 0;
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
    bool enable_vim_mode = false;  // Vim keybindings disabled until stable
    bool word_wrap = false;  // Default: off (requires TextEditor library patch)
    bool show_line_numbers = false;
    bool show_bookmark_margin = false;
    bool show_code_folding = true;
    bool show_change_history = false;
    bool show_minimap = false;
    bool show_spaces = false;
    bool show_tabs = true;
    bool explorer_left = false;  // Explorer position: true=left, false=right
    int terminal_position = 0;  // 0=bottom, 1=left, 2=top, 3=right
    int tab_size = 4;
    int font_size = 18;
    int highlight_line = 1;  // 0=none, 1=background, 2=outline
    std::string font_name = "";
    std::vector<std::string> recent_files;
    std::string last_open_dir;
    
    // Notifications
    bool notifications_enabled = true;        // Enable/disable all notifications
    int notification_duration = 4000;         // Duration in ms (0=persistent)
    bool notification_sound = true;          // Play sound on notification
    
    // Robustness & Self-healing
    bool auto_save = true;                    // Auto-save drafts
    int auto_save_interval = 30;             // Seconds between auto-save
    int max_undo_levels = 1000;            // Undo history depth
    bool create_backups = true;             // Create .bak files
    bool validate_settings = true;          // Validate on load
    
    // Monitoring & Diagnostics
    bool crash_reporting = true;           // Enable crash reports
    bool usage_telemetry = false;          // Anonymous usage stats
    bool health_checks = true;           // Periodic health checks
    
    // Forgiving UX
    bool confirm_quit = false;            // Confirm before quit with unsaved
    bool confirm_delete = true;            // Confirm file delete
    bool confirm_overwrite = true;       // Confirm overwrite
    
    // User-centric preferences
    bool remember_window_pos = true;       // Restore window position
    bool remember_open_files = true;     // Restore open files on start
    int max_recent_files = 10;
    
    // Maintenance
    int max_backup_files = 5;           // Keep up to 5 backups
    bool auto_update_check = true;       // Check for updates
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
    
    // Robustness & Self-healing
    void auto_save_timer();
    void create_backup(const std::string& path);
    bool validate_settings();
    void recover_from_crash();
    void run_health_check();
    
    // Monitoring & Assessment
    void log_event(const std::string& event);
    void check_for_updates();
    std::string get_diagnostics();
    void test_notifications();

    // Rendering
    void render();
    void validate_layout();
    void create_native_status_bar();
    void update_native_status_bar();
    void render_menu_bar();
    void render_menu_file();
    void render_menu_edit();
    void render_menu_view();
    void render_menu_view_zoom();
    void render_menu_recent();
    void render_editor_area();
    void render_status_bar();
    void render_command_line();
    void render_floating_command();
    void render_terminal();
    void start_terminal();
    void update_terminal_output();
    void render_sidebar();
    void render_file_tree();
    void render_dir_tree(const std::string& dir_path, std::unordered_map<std::string, bool>& expanded_dirs, const std::string& filter = "");
    void render_git_changes();
    void render_breadcrumb();
    void render_symbol_outline();
    void render_find_dialog();
    void render_replace_dialog();
    void render_goto_dialog();
    void render_command_palette();
    void render_font_dialog();
    void render_spaces_dialog();
    void render_about_dialog();

    // Menu
    void render_menu_help();
    void render_menu_split();
    void render_menu_options();

    // File operations
    void new_tab();
    void new_terminal_tab();
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

    // Version
    static std::string get_version();
    
    // View
    void zoom_in();
    void zoom_out();
    void zoom_reset();
    void terminal_zoom_in();
    void terminal_zoom_out();
    void terminal_zoom_reset();
    void toggle_status_bar();
    void toggle_explorer();
    void toggle_word_wrap();
    void toggle_line_numbers();
    void toggle_spaces();
    void toggle_minimap();
    void toggle_code_folding();
    void render_minimap(TextEditor* editor);
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

    // Vim mode
    void handle_vim_key(int key, int mods);
    bool is_vim_normal_mode() const { return vim_mode_ == VimMode::Normal; }
    const char* get_vim_mode_str() const;

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
    void* native_status_bar = nullptr;  // Native Windows status bar HWND
    bool running_ = true;
    std::vector<std::string> activity_log_;  // Session activity audit trail (non-persistent)
    EditorNotifications::NotificationManager* notification_manager_ = nullptr;

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
    bool show_about_ = false;
    bool show_style_editor_ = false;
    
    // Sidebar panels
    bool show_file_tree_ = true;
    bool explorer_pinned_ = false;  // true = always visible, false = hide in horizontal splits
    int explorer_side_ = 0;  // 0=left, 1=right, -1=hidden
    bool show_git_changes_ = false;
    bool show_symbol_outline_ = false;
    std::string git_branch_ = "main";
    std::vector<std::string> git_modified_files_;

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
    int pending_close_tab_idx_ = -1;  // Tab waiting for close confirmation
    bool show_new_file_dialog_ = false;
    bool show_new_folder_dialog_ = false;
    bool show_terminal_ = false;
    bool terminal_focused_ = false;
    bool terminal_input_active_ = false;
    void* terminal_hwnd_ = nullptr;
    void* terminal_process_ = nullptr;
    void* terminal_stdin_ = nullptr;
    void* terminal_stdout_ = nullptr;
    std::string terminal_output_;
#ifdef _WIN32
    void* conpty_handle_ = nullptr;
#endif
    int tab_size_temp_ = 4;
    int terminal_zoom_pct_ = 100;

    // Command palette
    char cmd_buf_[256] = {0};

// Vim mode state
    enum class VimMode { Normal, Insert, Visual, VisualLine, VisualBlock, OperatorPending, Command };
    VimMode vim_mode_ = VimMode::Normal;
    std::string vim_command_buffer_;
    char vim_cmd_input_[256] = {0};
    bool editor_focused_ = true;
    bool skip_texteditor_input_ = false;
    std::vector<std::string> terminal_history_;
    std::vector<std::string> command_history_;
    int history_index_ = -1;
    int vim_count_ = 0;
    int vim_operator_ = 0;
    char vim_command_input_[256] = "";
    bool execute_vim_command(const std::string& cmd);
    std::string vim_key_buffer_;
    
    // Vim character search state (f, F, t, T)
    char vim_last_find_char_ = 0;
    bool vim_last_find_reverse_ = false;
    bool vim_find_pending_ = false;  // true when waiting for search char
    
    // Visual Block mode state
    int visual_block_start_line_ = 0;
    int visual_block_start_col_ = 0;
    
    // Vim motion helpers
    void vim_move_to_line_start();
    void vim_move_to_line_end();
    int vim_count_words_forward(int line, int col);
    int vim_count_words_backward(int line, int col);
    
    // Splits
    TextEditor* get_active_editor();
    void split_horizontal();
    void split_vertical();
    void split_horizontal(bool for_terminal);
    void split_vertical(bool for_terminal);
    void start_split_terminal(Split* split);
    void close_split();
    void next_split();
    void prev_split();
    void open_file_split(const std::string& path);
    void open_file_split_vertical(const std::string& path);
    void rotate_splits();
    void equalize_splits();
    void render_splits(int tab_idx);
};
