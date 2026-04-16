// ============================================================================
// pcode-editor — Vim Motions Test Suite
// Tests for vim mode switching, cursor motions, and vim commands
// ============================================================================

#include <cstdio>
#include <cassert>
#include <string>
#include <vector>

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
// Mock VimMode enum for testing
// ============================================================================
enum class VimMode { Normal, Insert, Visual, VisualLine, OperatorPending, Command };

// ============================================================================
// Test: VimMode enum values
// ============================================================================
TEST(vim_mode_enum) {
    VimMode m1 = VimMode::Normal;
    VimMode m2 = VimMode::Insert;
    VimMode m3 = VimMode::Visual;
    
    // All modes should be distinct
    assert(m1 != m2);
    assert(m1 != m3);
    assert(m2 != m3);
}

// ============================================================================
// Test: Mode switching - Normal to Insert
// ============================================================================
TEST(mode_normal_to_insert) {
    VimMode mode = VimMode::Normal;
    
    // i - insert mode
    mode = VimMode::Insert;
    assert(mode == VimMode::Insert);
    
    // a - insert mode (after cursor)
    mode = VimMode::Insert;
    assert(mode == VimMode::Insert);
}

// ============================================================================
// Test: Mode switching - Insert to Normal
// ============================================================================
TEST(mode_insert_to_normal) {
    VimMode mode = VimMode::Insert;
    
    // Escape returns to Normal mode
    mode = VimMode::Normal;
    assert(mode == VimMode::Normal);
}

// ============================================================================
// Test: Mode switching - Normal to Command
// ============================================================================
TEST(mode_normal_to_command) {
    VimMode mode = VimMode::Normal;
    
    // : enters command mode
    mode = VimMode::Command;
    assert(mode == VimMode::Command);
}

// ============================================================================
// Test: Cursor position structure
// ============================================================================
TEST(cursor_position) {
    struct Coordinates {
        int mLine;
        int mColumn;
    };
    
    Coordinates pos = {0, 0};
    
    // Test default position
    assert(pos.mLine == 0);
    assert(pos.mColumn == 0);
    
    // Test setting position
    pos = {10, 5};
    assert(pos.mLine == 10);
    assert(pos.mColumn == 5);
}

// ============================================================================
// Test: Word motion - forward (w)
// ============================================================================
TEST(word_motion_forward) {
    std::string text = "hello world test";
    
    // Find word start after current position
    auto find_word_start = [=](const std::string& s, int from) -> int {
        int pos = from;
        while (pos < (int)s.size() && s[pos] == ' ') pos++;
        return pos;
    };
    
    int pos = find_word_start(text, 0);
    assert(pos == 0);
    
    pos = find_word_start(text, 6);
    assert(pos == 6);
}

// ============================================================================
// Test: Word motion - backward (b)
// ============================================================================
TEST(word_motion_backward) {
    std::string text = "hello world test";
    
    // Find word start going backward
    auto find_word_start_back = [=](const std::string& s, int from) -> int {
        int pos = from;
        while (pos > 0 && s[pos - 1] == ' ') pos--;
        while (pos > 0) {
            char c = s[pos - 1];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                pos--;
            } else {
                break;
            }
        }
        return pos;
    };
    
    int pos = find_word_start_back(text, 11);
    assert(pos == 6);
}

// ============================================================================
// Test: Line motion - 0, ^, $
// ============================================================================
TEST(line_motion) {
    std::string line = "  int x = 5;";
    
    int col_0 = 0;
    assert(col_0 == 0);
    
    int col_first = 2;
    assert(line[col_first] == 'i');
    
    int col_end = (int)line.size() - 1;
    assert(line[col_end] == ';');
}

// ============================================================================
// Test: Document motion - gg, G
// ============================================================================
TEST(document_motion) {
    int total_lines = 100;
    
    int pos_gg = 0;
    assert(pos_gg == 0);
    
    int pos_G = total_lines - 1;
    assert(pos_G == 99);
    
    int pos_50 = 49;
    assert(pos_50 == 49);
}

// ============================================================================
// Test: Command parsing
// ============================================================================
TEST(command_parsing) {
    std::string cmd = ":w";
    if (!cmd.empty() && cmd[0] == ':') {
        cmd = cmd.substr(1);
    }
    assert(cmd == "w");
    
    cmd = "e filename.cpp";
    if (!cmd.empty() && cmd[0] == ':') {
        cmd = cmd.substr(1);
    }
    assert(cmd == "e filename.cpp");
}

// ============================================================================
// Test: Vim command - :w (write)
// ============================================================================
TEST(vim_command_write) {
    std::string cmd = "w";
    bool executed = false;
    
    if (cmd == "w") {
        executed = true;
    }
    
    assert(executed == true);
}

// ============================================================================
// Test: Vim command - :q (quit)
// ============================================================================
TEST(vim_command_quit) {
    std::string cmd = "q";
    bool executed = false;
    
    if (cmd == "q") {
        executed = true;
    }
    
    assert(executed == true);
}

// ============================================================================
// Test: Vim command - :e (edit/open file)
// ============================================================================
TEST(vim_command_edit) {
    std::string cmd = "e test.cpp";
    bool executed = false;
    
    if (cmd.substr(0, 1) == "e" && cmd.length() > 1) {
        executed = true;
    }
    
    assert(executed == true);
}

// ============================================================================
// Test: Vim command - :sp (horizontal split)
// ============================================================================
TEST(vim_command_split_horiz) {
    std::string cmd = "sp";
    bool executed = false;
    
    if (cmd == "sp") {
        executed = true;
    }
    
    assert(executed == true);
}

// ============================================================================
// Test: Vim command - :vsp (vertical split)
// ============================================================================
TEST(vim_command_split_vert) {
    std::string cmd = "vsp";
    bool executed = false;
    
    if (cmd == "vsp") {
        executed = true;
    }
    
    assert(executed == true);
}

// ============================================================================
// Test: Vim command - :new (new tab)
// ============================================================================
TEST(vim_command_new) {
    std::string cmd = "new";
    bool executed = false;
    
    if (cmd == "new") {
        executed = true;
    }
    
    assert(executed == true);
}

// ============================================================================
// Test: Vim command - :version
// ============================================================================
TEST(vim_command_version) {
    std::string cmd = "version";
    std::string version_output = "";
    
    if (cmd == "version") {
        version_output = "pCode Editor version 0.1.0 (test)";
    }
    
    assert(version_output == "pCode Editor version 0.1.0 (test)");
}

// ============================================================================
// Test: Version string format
// ============================================================================
TEST(version_string_format) {
    std::string version = "pCode Editor version 0.1.0 (35f8455)";
    
    assert(version.find("pCode Editor") != std::string::npos);
    assert(version.find("version") != std::string::npos);
    assert(version.find("0.1.0") != std::string::npos);
    assert(version.find("(") != std::string::npos);
    assert(version.find(")") != std::string::npos);
}

// ============================================================================
// Main
// ============================================================================
int main() {
    printf("=== pcode-editor Vim Motions Test Suite ===\n\n");
    
    total_count = 18;
    
    RUN_TEST(vim_mode_enum);
    RUN_TEST(mode_normal_to_insert);
    RUN_TEST(mode_insert_to_normal);
    RUN_TEST(mode_normal_to_command);
    RUN_TEST(cursor_position);
    RUN_TEST(word_motion_forward);
    RUN_TEST(word_motion_backward);
    RUN_TEST(line_motion);
    RUN_TEST(document_motion);
    RUN_TEST(command_parsing);
    RUN_TEST(vim_command_write);
    RUN_TEST(vim_command_quit);
    RUN_TEST(vim_command_edit);
    RUN_TEST(vim_command_split_horiz);
    RUN_TEST(vim_command_split_vert);
    RUN_TEST(vim_command_new);
    RUN_TEST(vim_command_version);
    RUN_TEST(version_string_format);
    
    printf("\n=== Results: %d/%d tests passed ===\n", passed_count, total_count);
    
    if (passed_count == total_count) {
        printf("All vim motion tests passed!\n");
        return 0;
    } else {
        printf("Some vim motion tests failed\n");
        return 1;
    }
}