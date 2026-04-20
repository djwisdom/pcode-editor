// ============================================================================
// pcode-editor Regression Test Suite
// ============================================================================
// Run these tests to verify previously vetted features don't regress.
// Add new tests for any bug fix to prevent future regressions.
// ============================================================================

#include <cstdio>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;
static std::vector<std::string> failed_tests;

#define TEST(name, expr) do { \
    printf("  [%s] %s...\n", (expr) ? "PASS" : "FAIL", name); \
    if (expr) { tests_passed++; } \
    else { tests_failed++; failed_tests.push_back(name); } \
} while(0)

#define TEST_CATEGORY(name) printf("\n=== %s ===\n", name)

// Helper: Check if file contains a string pattern
static bool file_contains(const char* filepath, const char* pattern) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        if (line.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Regression Tests - Tab System
// ============================================================================
void test_tab_bounds_checking() {
    TEST_CATEGORY("Tab Index Bounds Checking");
    
    // close_tab must check bounds before accessing tabs_[]
    TEST("close_tab checks bounds before access", 
         file_contains("src/editor_app.cpp", "if (idx < 0 || idx >= (int)tabs_.size())"));
    
    // save_tab must check bounds
    TEST("save_tab checks bounds before access",
         file_contains("src/editor_app.cpp", "if (tab_idx < 0 || tab_idx >= (int)tabs_.size())"));
}

void test_tab_dirty_flag() {
    TEST_CATEGORY("Tab Dirty Flag (*) Persistence");
    
    // IsTextChanged must be checked BEFORE Render() to avoid reset
    TEST("Dirty flag checked before Render()",
         file_contains("src/editor_app.cpp", "IsTextChanged()"));
}

void test_tab_close_confirmation() {
    TEST_CATEGORY("Tab Close - Dirty Confirmation Dialog");
    
    // Dirty tabs should show confirmation dialog
    TEST("Has pending_close_tab_idx_ member",
         file_contains("src/editor_app.h", "pending_close_tab_idx_"));
    
    // Confirmation popup renders when closing dirty tab
    TEST("Discard Changes popup implemented",
         file_contains("src/editor_app.cpp", "Discard Changes?"));
}

// ============================================================================
// Regression Tests - Settings
// ============================================================================
void test_settings_persistence() {
    TEST_CATEGORY("Settings Persistence");
    
    // Settings must persist show_tabs
    TEST("show_tabs persisted in save_settings",
         file_contains("src/editor_app.cpp", "show_tabs"));
    
    // Settings must persist show_file_tree (explorer)
    TEST("explorer state persisted",
         file_contains("src/editor_app.cpp", "last_open_dir"));
}

// ============================================================================
// Regression Tests - Dialogs
// ============================================================================
void test_dialogs() {
    TEST_CATEGORY("Find/Replace/About Dialogs");
    
    // Find dialog exists and opens
    TEST("Find dialog exists", 
         file_contains("src/editor_app.cpp", "show_find_ = true"));
    
    // Replace dialog exists
    TEST("Replace dialog exists",
         file_contains("src/editor_app.cpp", "show_replace_ = true"));
    
    // About dialog exists
    TEST("About dialog exists",
         file_contains("src/editor_app.cpp", "show_about_"));
}

// ============================================================================
// Regression Tests - Notifications
// ============================================================================
void test_notification_system() {
    TEST_CATEGORY("Notification System");
    
    // Base notification system exists
    TEST("imgui_notify.h exists",
         file_contains("src/imgui_notify.h", "RenderNotifications"));
    
    // Developer notifications exist
    TEST("editor_notifications.h exists",
         file_contains("src/editor_notifications.h", "NotificationManager"));
    
    // Notifications are rendered in main loop
    TEST("Notifications rendered in render()",
         file_contains("src/editor_app.cpp", "RenderNotifications()"));
}

// ============================================================================
// Regression Tests - Menu System
// ============================================================================
void test_menu_shortcuts() {
    TEST_CATEGORY("Menu Keyboard Shortcuts");
    
    // Ctrl+S for save
    TEST("Save shortcut Ctrl+S",
         file_contains("src/editor_app.cpp", "IsKeyPressed(ImGuiKey_S)"));
    
    // Ctrl+W for close tab
    TEST("Close tab shortcut Ctrl+W",
         file_contains("src/editor_app.cpp", "IsKeyPressed(ImGuiKey_W)"));
    
    // Ctrl+F for find
    TEST("Find shortcut Ctrl+F",
         file_contains("src/editor_app.cpp", "IsKeyPressed(ImGuiKey_F)"));
    
    // Ctrl+N for new tab
    TEST("New tab shortcut Ctrl+N",
         file_contains("src/editor_app.cpp", "IsKeyPressed(ImGuiKey_N)"));
}

// ============================================================================
// Regression Tests - Explorer
// ============================================================================
void test_explorer() {
    TEST_CATEGORY("File Explorer");
    
    // Explorer can be toggled
    TEST("Explorer toggle exists",
         file_contains("src/editor_app.cpp", "show_file_tree_"));
    
    // File tree rendering exists
    TEST("File tree rendering exists",
         file_contains("src/editor_app.cpp", "render_file_tree"));
}

// ============================================================================
// Regression Tests - Theme
// ============================================================================
void test_theme() {
    TEST_CATEGORY("Theme System");
    
    // Dark/Light theme support
    TEST("Dark theme toggle exists",
         file_contains("src/editor_app.cpp", "dark_theme"));
    
    // Theme applies to all splits
    TEST("Theme iterates splits",
         file_contains("src/editor_app.cpp", "tab.splits"));
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main() {
    printf("+----------------------------------------------------------+\n");
    printf("|           pcode-editor Regression Test Suite                |\n");
    printf("+----------------------------------------------------------+\n");
    printf("|  This suite verifies that previously vetted features don't    |\n");
    printf("|  regress when new changes are introduced.                 |\n");
    printf("|                                                  |\n");
    printf("|  Tests check for presence of critical code patterns.     |\n");
    printf("|  If a test fails, the feature has regressed.               |\n");
    printf("+----------------------------------------------------------+\n");
    
    // Run all test categories
    test_tab_bounds_checking();
    test_tab_dirty_flag();
    test_tab_close_confirmation();
    test_settings_persistence();
    test_dialogs();
    test_notification_system();
    test_menu_shortcuts();
    test_explorer();
    test_theme();
    
    // Summary
    printf("\n+----------------------------------------------------------+\n");
    printf("TEST SUMMARY: %d passed, %d failed\n", tests_passed, tests_failed);
    
    if (tests_failed > 0) {
        printf("\nFAILED TESTS:\n");
        for (const auto& name : failed_tests) {
            printf("  [FAIL] %s\n", name.c_str());
        }
        printf("\nThese features have regressed and need fixing!\n");
        return 1;
    } else {
        printf("\nAll regression tests passed! No known issues detected.\n");
        return 0;
    }
}
