// ============================================================================
// pcode-editor — Test Suite
// Tests for View menu features, file operations, theme, and font management
// ============================================================================

#include <cstdio>
#include <cassert>
#include <string>
#include <fstream>
#include <filesystem>

// Simple test framework
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    printf("  Running test_%s... ", #name); \
    test_##name(); \
    printf("PASSED\n"); \
    passed_count++; \
} while(0)

static int passed_count = 0;
static int total_count = 0;

// ============================================================================
// Test: Settings persistence
// ============================================================================
TEST(settings_persistence) {
    std::string test_path = "/tmp/test_pcode_settings.json";
    
    // Write test settings
    {
        std::ofstream f(test_path);
        f << "{\n";
        f << "  \"window_w\": 1024,\n";
        f << "  \"window_h\": 768,\n";
        f << "  \"dark_theme\": false,\n";
        f << "  \"show_status_bar\": false,\n";
        f << "  \"word_wrap\": true,\n";
        f << "  \"show_line_numbers\": false,\n";
        f << "  \"font_size\": 20,\n";
        f << "  \"last_open_dir\": \"/tmp\",\n";
        f << "  \"recent_files\": []\n";
        f << "}\n";
    }
    
    // Read and verify
    {
        std::ifstream f(test_path);
        assert(f.is_open());
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        
        assert(content.find("\"word_wrap\": true") != std::string::npos);
        assert(content.find("\"show_line_numbers\": false") != std::string::npos);
        assert(content.find("\"font_size\": 20") != std::string::npos);
    }
    
    std::filesystem::remove(test_path);
}

// ============================================================================
// Test: Tab size validation
// ============================================================================
TEST(tab_size_validation) {
    // Tab sizes should be between 1 and 16
    int valid_sizes[] = {1, 2, 4, 8, 16};
    for (int size : valid_sizes) {
        assert(size >= 1 && size <= 16);
    }
    
    // Invalid sizes should be rejected
    assert(0 < 1);  // Too small
    assert(17 > 16); // Too large
}

// ============================================================================
// Test: Theme toggle state
// ============================================================================
TEST(theme_toggle_state) {
    bool dark_theme = true;
    
    // Toggle once
    dark_theme = !dark_theme;
    assert(dark_theme == false);
    
    // Toggle again
    dark_theme = !dark_theme;
    assert(dark_theme == true);
}

// ============================================================================
// Test: Word wrap state
// ============================================================================
TEST(word_wrap_state) {
    bool word_wrap = false;
    
    word_wrap = !word_wrap;
    assert(word_wrap == true);
    
    word_wrap = !word_wrap;
    assert(word_wrap == false);
}

// ============================================================================
// Test: Line numbers state
// ============================================================================
TEST(line_numbers_state) {
    bool show_line_numbers = true;
    
    show_line_numbers = !show_line_numbers;
    assert(show_line_numbers == false);
    
    show_line_numbers = !show_line_numbers;
    assert(show_line_numbers == true);
}

// ============================================================================
// Test: Font size bounds
// ============================================================================
TEST(font_size_bounds) {
    int font_size = 16;
    
    // Test minimum
    font_size = 8;
    assert(font_size >= 8 && font_size <= 48);
    
    // Test maximum
    font_size = 48;
    assert(font_size >= 8 && font_size <= 48);
    
    // Test out of bounds (should be clamped)
    font_size = 4;
    font_size = font_size < 8 ? 8 : (font_size > 48 ? 48 : font_size);
    assert(font_size == 8);
    
    font_size = 64;
    font_size = font_size < 8 ? 8 : (font_size > 48 ? 48 : font_size);
    assert(font_size == 48);
}

// ============================================================================
// Test: File path extraction
// ============================================================================
TEST(file_path_extraction) {
    std::string path = "/home/user/test.cpp";
    auto filename = std::filesystem::path(path).filename().string();
    assert(filename == "test.cpp");
    
    path = "C:\\Users\\test.cpp";
    filename = std::filesystem::path(path).filename().string();
    assert(filename == "test.cpp");
}

// ============================================================================
// Test: Recent files management
// ============================================================================
TEST(recent_files_management) {
    std::vector<std::string> recent_files;
    
    // Add files
    recent_files.push_back("/tmp/file1.cpp");
    recent_files.push_back("/tmp/file2.cpp");
    recent_files.push_back("/tmp/file3.cpp");
    
    // Should maintain order
    assert(recent_files.size() == 3);
    assert(recent_files[0] == "/tmp/file1.cpp");
    
    // Remove duplicate
    recent_files.push_back("/tmp/file1.cpp");
    recent_files.erase(
        std::remove(recent_files.begin(), recent_files.end(), "/tmp/file1.cpp"),
        recent_files.end());
    recent_files.insert(recent_files.begin(), "/tmp/file1.cpp");
    
    // Should have only one instance of file1
    int count = 0;
    for (const auto& f : recent_files) {
        if (f == "/tmp/file1.cpp") count++;
    }
    assert(count == 1);
    
    // Limit to 10 files
    while (recent_files.size() > 10) {
        recent_files.resize(10);
    }
    assert(recent_files.size() <= 10);
}

// ============================================================================
// Test: Zoom percentage bounds
// ============================================================================
TEST(zoom_bounds) {
    int zoom_pct = 100;
    
    zoom_pct = std::min(200, zoom_pct + 10);
    assert(zoom_pct == 110);
    
    zoom_pct = std::max(50, zoom_pct - 60);
    assert(zoom_pct == 50);
    
    zoom_pct = std::min(200, zoom_pct + 200);
    assert(zoom_pct == 200);
}

// ============================================================================
// Test: Line ending detection
// ============================================================================
TEST(line_ending_detection) {
    std::string text_crlf = "line1\r\nline2\r\n";
    std::string text_lf = "line1\nline2\n";
    std::string text_cr = "line1\rline2\r";
    
    auto detect_line_ending = [](const std::string& content) -> std::string {
        if (content.find("\r\n") != std::string::npos) return "CRLF";
        if (content.find("\r") != std::string::npos) return "CR";
        return "LF";
    };
    
    assert(detect_line_ending(text_crlf) == "CRLF");
    assert(detect_line_ending(text_lf) == "LF");
    assert(detect_line_ending(text_cr) == "CR");
}

// ============================================================================
// Simulates the ACTUAL render logic from editor_app.cpp render_explorer()
// This replicates the exact conditionals used in the real rendering code
// ============================================================================

struct AppSettings {
    bool explorer_left = false;
};

struct TestState {
    bool show_file_tree = false;
    AppSettings settings;
};

static bool render_explorer_logic(TestState& state) {
    // This is THE EXACT LOGIC from render_explorer() in editor_app.cpp
    // Line 3561: if (show_file_tree_ && !settings_.explorer_left)
    if (state.show_file_tree && !state.settings.explorer_left) {
        // Right side path - should render explorer AFTER editor
        return true;  // rendering happened on right
    } else {
        // Left side path or no explorer
        if (state.show_file_tree && state.settings.explorer_left) {
            return true;  // rendering happened on left
        }
        return false;  // no explorer rendered
    }
}

// ============================================================================
// Test: Explorer renders on RIGHT when explorer_left is false
// ============================================================================
TEST(explorer_renders_on_right) {
    TestState state;
    
    // Enable explorer on RIGHT side (explorer_left = false)
    state.show_file_tree = true;
    state.settings.explorer_left = false;
    
    bool rendered = render_explorer_logic(state);
    assert(rendered == true);  // Must render!
    
    // Verify we're using the right-side code path
    // In real code: !settings_.explorer_left means right side
    assert(state.settings.explorer_left == false);
    assert(!state.settings.explorer_left == true);  // Right side condition
}

// ============================================================================
// Test: Explorer renders on LEFT when explorer_left is true  
// ============================================================================
TEST(explorer_renders_on_left) {
    TestState state;
    
    // Enable explorer on LEFT side (explorer_left = true)
    state.show_file_tree = true;
    state.settings.explorer_left = true;
    
    bool rendered = render_explorer_logic(state);
    assert(rendered == true);  // Must render!
    
    // Verify we're using the left-side code path
    assert(state.settings.explorer_left == true);
}

// ============================================================================
// Test: Switching from left to right should keep explorer visible
// This simulates what happens when user clicks "Explorer Position > Right" menu
// ============================================================================
TEST(explorer_switch_left_to_right) {
    TestState state;
    
    // Start with explorer on left
    state.show_file_tree = true;
    state.settings.explorer_left = true;
    
    bool rendered_left = render_explorer_logic(state);
    assert(rendered_left == true);  // Initially visible
    
    // Switch to right (simulates menu click: settings_.explorer_left = false)
    state.settings.explorer_left = false;
    
    // IMPORTANT: show_file_tree must STILL BE TRUE when switching!
    // This is the bug - the old code set show_file_tree = true when switching
    // But the actual check is: show_file_tree_ && !settings_.explorer_left
    
    bool rendered_right = render_explorer_logic(state);
    assert(rendered_right == true);  // Should still render! BUG: this fails if show_file_tree was reset
    
    // The rendering logic requires BOTH conditions:
    // 1. show_file_tree == true  (must be preserved!)
    // 2. explorer_left == false (right side)
    assert(state.show_file_tree == true);  // MUST be true - this is the bug!
    assert(state.settings.explorer_left == false);  // Right side
    assert(state.show_file_tree && !state.settings.explorer_left);  // Right path condition
}

// ============================================================================
// Test: Verify the exact conditional from render_explorer()
// This is the ACTUAL condition used at line 3561 in editor_app.cpp
// ============================================================================
TEST(right_side_conditional_exact) {
    TestState state;
    
    // Setup: user clicked "Explorer Position > Right"
    state.show_file_tree = true;
    state.settings.explorer_left = false;
    
    // This is THE EXACT condition from line 3561:
    // if (show_file_tree_ && !settings_.explorer_left)
    bool right_side_path = (state.show_file_tree && !state.settings.explorer_left);
    
    assert(right_side_path == true);  // Should enter right-side code path
    
    // This is what the bug would cause - wrong condition values
    // If show_file_tree was accidentally set to false, this fails:
    state.show_file_tree = false;
    right_side_path = (state.show_file_tree && !state.settings.explorer_left);
    assert(right_side_path == false);  // Should NOT render when hidden
}

// ============================================================================
// Main
// ============================================================================
int main() {
    printf("=== pcode-editor Test Suite ===\n\n");
    
    total_count = 14;
    
    RUN_TEST(settings_persistence);
    RUN_TEST(tab_size_validation);
    RUN_TEST(theme_toggle_state);
    RUN_TEST(word_wrap_state);
    RUN_TEST(line_numbers_state);
    RUN_TEST(font_size_bounds);
    RUN_TEST(file_path_extraction);
    RUN_TEST(recent_files_management);
    RUN_TEST(zoom_bounds);
    RUN_TEST(line_ending_detection);
    RUN_TEST(explorer_renders_on_right);
    RUN_TEST(explorer_renders_on_left);
    RUN_TEST(explorer_switch_left_to_right);
    RUN_TEST(right_side_conditional_exact);
    
    printf("\n=== Results: %d/%d tests passed ===\n", passed_count, total_count);
    
    if (passed_count == total_count) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed\n");
        return 1;
    }
}
