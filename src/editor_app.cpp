// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 pCode Editor Development Team
// Author: Dennis O. Esternon <djwisdom@serenityos.org>
// Contributors: see CONTRIBUTORS or git history

// ============================================================================
// pcode-editor — Personal Code Editor
// Dear ImGui + GLFW + ImGuiColorTextEdit + Native File Dialog
// ============================================================================

#include "editor_app.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <TextEditor.h>
#include <nfd.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdio>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// Simple JSON helpers (no external dependency)
// ============================================================================
static std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

static std::string json_unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i + 1] == '\\') { out += '\\'; i++; }
            else if (s[i + 1] == '"') { out += '"'; i++; }
            else out += s[i];
        } else {
            out += s[i];
        }
    }
    return out;
}

static void settings_save(const AppSettings& s, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "{\n";
    f << "  \"window_w\": " << s.window_w << ",\n";
    f << "  \"window_h\": " << s.window_h << ",\n";
    f << "  \"dark_theme\": " << (s.dark_theme ? "true" : "false") << ",\n";
    f << "  \"show_status_bar\": " << (s.show_status_bar ? "true" : "false") << ",\n";
    f << "  \"word_wrap\": " << (s.word_wrap ? "true" : "false") << ",\n";
    f << "  \"show_line_numbers\": " << (s.show_line_numbers ? "true" : "false") << ",\n";
    f << "  \"show_minimap\": " << (s.show_minimap ? "true" : "false") << ",\n";
    f << "  \"show_spaces\": " << (s.show_spaces ? "true" : "false") << ",\n";
    f << "  \"highlight_line\": " << s.highlight_line << ",\n";
    f << "  \"show_tabs\": " << (s.show_tabs ? "true" : "false") << ",\n";
    f << "  \"tab_size\": " << s.tab_size << ",\n";
    f << "  \"font_size\": " << s.font_size << ",\n";
    f << "  \"font_name\": \"" << json_escape(s.font_name) << "\",\n";
    f << "  \"last_open_dir\": \"" << json_escape(s.last_open_dir) << "\",\n";
    f << "  \"recent_files\": [";
    for (size_t i = 0; i < s.recent_files.size(); i++) {
        if (i > 0) f << ", ";
        f << "\"" << json_escape(s.recent_files[i]) << "\"";
    }
    f << "]\n}\n";
}

static void settings_load(AppSettings& s, const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    auto get_str = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        auto pos = content.find(search);
        if (pos == std::string::npos) return "";
        auto colon = content.find(':', pos);
        if (colon == std::string::npos) return "";
        auto q1 = content.find('"', colon);
        if (q1 == std::string::npos) return "";
        auto q2 = content.find('"', q1 + 1);
        if (q2 == std::string::npos) return "";
        return json_unescape(content.substr(q1 + 1, q2 - q1 - 1));
    };
    auto get_int = [&](const std::string& key, int def) -> int {
        std::string search = "\"" + key + "\"";
        auto pos = content.find(search);
        if (pos == std::string::npos) return def;
        auto colon = content.find(':', pos);
        if (colon == std::string::npos) return def;
        try { return std::stoi(content.substr(colon + 1)); } catch (...) { return def; }
    };
    auto get_bool = [&](const std::string& key, bool def) -> bool {
        std::string search = "\"" + key + "\"";
        auto pos = content.find(search);
        if (pos == std::string::npos) return def;
        return content.find("true", pos) != std::string::npos;
    };

    s.window_w = get_int("window_w", 1280);
    s.window_h = get_int("window_h", 800);
    s.dark_theme = get_bool("dark_theme", true);
    s.show_status_bar = get_bool("show_status_bar", true);
    s.word_wrap = get_bool("word_wrap", false);
    s.show_line_numbers = get_bool("show_line_numbers", true);
    s.show_bookmark_margin = get_bool("show_bookmark_margin", true);
    s.show_change_history = get_bool("show_change_history", true);
    s.show_minimap = get_bool("show_minimap", true);
    s.show_spaces = get_bool("show_spaces", false);
    s.highlight_line = get_int("highlight_line", 1);
    s.show_tabs = get_bool("show_tabs", true);
    s.tab_size = get_int("tab_size", 4);
    s.font_size = get_int("font_size", 18);
    s.font_name = get_str("font_name");
    s.last_open_dir = get_str("last_open_dir");

    // Parse recent files array
    auto arr_start = content.find("\"recent_files\"");
    if (arr_start != std::string::npos) {
        auto bracket = content.find('[', arr_start);
        auto bracket_end = content.find(']', bracket);
        if (bracket != std::string::npos && bracket_end != std::string::npos) {
            std::string arr = content.substr(bracket + 1, bracket_end - bracket - 1);
            size_t pos = 0;
            while ((pos = arr.find('"', pos)) != std::string::npos) {
                auto end = arr.find('"', pos + 1);
                if (end == std::string::npos) break;
                s.recent_files.push_back(json_unescape(arr.substr(pos + 1, end - pos - 1)));
                pos = end + 1;
            }
        }
    }
}

// ============================================================================
// Version
// ============================================================================
std::string EditorApp::get_version() {
    return "pCode Editor version 0.2.31";
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
EditorApp::EditorApp(int argc, char* argv[])
    : argc_(argc), argv_(argv) {
    new_tab();  // Start with one untitled tab
    font_size_temp_ = settings_.font_size;
    tab_size_temp_ = settings_.tab_size;
    
    // Check for --version argument
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            printf("%s\n", get_version().c_str());
            running_ = false;
            return;
        }
        if (arg == "--help" || arg == "-h") {
            printf("Usage: pcode-editor [options] [files...]\n");
            printf("Options:\n");
            printf("  --version, -v  Show version info\n");
            printf("  --help, -h       Show this help\n");
            running_ = false;
            return;
        }
    }
}

EditorApp::~EditorApp() {
    for (auto& tab : tabs_) {
        delete tab.editor;
    }
}

void EditorApp::load_files_from_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        open_file(argv[i]);
    }
}

// ============================================================================
// Main loop
// ============================================================================
int EditorApp::run() {
    // If already exited due to --version or --help
    if (!running_) return 0;
    
    init();
    
    // Load files from command line arguments
    load_files_from_args(argc_, argv_);

    while (running_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        // Theme background
        if (settings_.dark_theme) {
            glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        } else {
            glClearColor(0.94f, 0.94f, 0.94f, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }

    save_settings();
    shutdown();
    return 0;
}

// ============================================================================
// Init / Shutdown
// ============================================================================
void EditorApp::init() {
    load_settings();

    // Initialize native file dialog
    NFD_Init();

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window_ = glfwCreateWindow(settings_.window_w, settings_.window_h, "pCode Editor", nullptr, nullptr);
    if (!window_) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        exit(1);
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Load fonts
    load_fonts();

    apply_theme(settings_.dark_theme);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Apply initial settings to first tab
    if (!tabs_.empty()) {
        TextEditor* ed = tabs_[0].editor;
        int tab_idx = 0;

        if (settings_.word_wrap) {
            ed->SetImGuiChildIgnored(true);
        }
        
        ed->SetTabSize(settings_.tab_size);
        ed->SetShowWhitespaces(settings_.show_spaces);
        // Note: TextEditor always shows line numbers (no toggle available in this version)
        
        apply_zoom(tab_idx);
    }
}

void EditorApp::shutdown() {
    NFD_Quit();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void EditorApp::load_settings() {
    std::string path;
#ifdef _WIN32
    path = std::filesystem::current_path().string() + "\\pcode-settings.json";
#else
    path = std::filesystem::current_path().string() + "/pcode-settings.json";
#endif
    settings_load(settings_, path);
}

void EditorApp::save_settings() {
    std::string path;
#ifdef _WIN32
    path = std::filesystem::current_path().string() + "\\pcode-settings.json";
#else
    path = std::filesystem::current_path().string() + "/pcode-settings.json";
#endif
    settings_save(settings_, path);
}

// ============================================================================
// Theme
// ============================================================================
void EditorApp::apply_theme(bool dark) {
    if (dark) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }
    // Tweak for editor feel
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
}

// ============================================================================
// Tab management
// ============================================================================
void EditorApp::new_tab() {
    EditorTab tab;
    tab.display_name = "untitled";
    tab.editor = new TextEditor();
    tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    tab.editor->SetTabSize(settings_.tab_size);

    tab.editor->SetText("");
    tabs_.push_back(std::move(tab));
    active_tab_ = (int)tabs_.size() - 1;
    
    // Create initial split for the tab
    Split* split = new Split();
    split->editor = get_active_editor();
    split->is_horizontal = true;
    split->ratio = 1.0f;
    tabs_[active_tab_].splits.push_back(split);
    tabs_[active_tab_].active_split = 0;
}

void EditorApp::new_window() {
    // Launch a new instance of the same executable
    // Using execl is safer than system()
#ifdef _WIN32
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    CreateProcess("pcode-editor.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
#else
    if (fork() == 0) {
        execl("./pcode-editor", "./pcode-editor", NULL);
    }
#endif
}

void EditorApp::open_file(const std::string& path) {
    std::string selected_path = path;

    if (path.empty()) {
        nfdchar_t* out_path = nullptr;
        // Pass null for defaultPath to let OS decide (avoids ShellItem error)
        nfdresult_t result = NFD_OpenDialog(&out_path, nullptr, 0, nullptr);

        if (result == NFD_OKAY && out_path) {
            selected_path = out_path;
            // Save the directory for next time
            settings_.last_open_dir = std::filesystem::path(out_path).parent_path().string();
            NFD_FreePath(out_path);
        } else if (result == NFD_CANCEL) {
            return;
        } else {
            fprintf(stderr, "NFD Error: %s\n", NFD_GetError());
            return;
        }
    }

    std::ifstream file(selected_path, std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open: %s\n", selected_path.c_str());
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Reuse current tab if it's untitled and clean
    bool reuse_tab = false;
    if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
        auto& t = tabs_[active_tab_];
        if (t.file_path.empty() && !t.dirty && t.editor->GetText().empty()) {
            reuse_tab = true;
        }
    }

    int tab_idx = active_tab_;
    if (!reuse_tab) {
        new_tab();
        tab_idx = (int)tabs_.size() - 1;
    }

    auto& tab = tabs_[tab_idx];
    tab.file_path = selected_path;
    tab.display_name = std::filesystem::path(selected_path).filename().string();
    tab.dirty = false;
    tab.editor->SetText(content);
    tab.editor->SetTabSize(settings_.tab_size);
    
    // Cache file info for status bar
    tab.file_encoding = "UTF-8";  // TODO: detect actual encoding
    tab.line_ending = get_line_ending(content);
    tab.file_size = content.size();

    // Auto-detect language
    auto ext = std::filesystem::path(selected_path).extension().string();
    if (ext == ".c" || ext == ".h") tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::C());
    else if (ext == ".cpp" || ext == ".hpp") tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    else if (ext == ".py" || ext == ".lua") tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    else if (ext == ".js" || ext == ".json") tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    else if (ext == ".sql") tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::SQL());
    else if (ext == ".glsl" || ext == ".hlsl") tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

    
    apply_zoom(tab_idx);

    settings_.last_open_dir = std::filesystem::path(selected_path).parent_path().string();
    add_recent_file(selected_path);
    
    // Set active tab to newly opened file
    active_tab_ = tab_idx;
    ImGui::SetWindowFocus("Editor");
}

bool EditorApp::save_tab(int idx) {
    if (idx < 0 || idx >= (int)tabs_.size()) return false;
    auto& tab = tabs_[idx];

    if (tab.file_path.empty()) {
        return save_tab_as(idx);
    }

    TextEditor* editor = tab.editor;
    std::ofstream file(tab.file_path, std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to save: %s\n", tab.file_path.c_str());
        return false;
    }
    file << editor->GetText();
    file.close();
    tab.dirty = false;
    tab.file_size = editor->GetText().size();
    tab.line_ending = get_line_ending(editor->GetText());
    add_recent_file(tab.file_path);
    return true;
}

bool EditorApp::save_tab_as(int idx) {
    if (idx < 0 || idx >= (int)tabs_.size()) return false;
    auto& tab = tabs_[idx];

    nfdchar_t* out_path = nullptr;
    nfdresult_t result = NFD_SaveDialog(&out_path, nullptr, 0,
        settings_.last_open_dir.empty() ? nullptr : settings_.last_open_dir.c_str(),
        tab.display_name.c_str());

    if (result != NFD_OKAY || !out_path) return false;

    tab.file_path = out_path;
    tab.display_name = std::filesystem::path(out_path).filename().string();
    NFD_FreePath(out_path);

    return save_tab(idx);
}

void EditorApp::save_all() {
    for (int i = 0; i < (int)tabs_.size(); i++) {
        if (tabs_[i].dirty) {
            save_tab(i);
        }
    }
}

bool EditorApp::confirm_discard_unsaved(int tab_idx) {
    if (tab_idx < 0 || tab_idx >= (int)tabs_.size()) return true;
    if (!tabs_[tab_idx].dirty) return true;

    // ImGui confirmation
    ImGui::OpenPopup("Discard Changes?");
    // We handle this via a modal in render
    return false;  // Caller should check ImGui popup
}

void EditorApp::close_tab(int idx) {
    if (idx < 0 || idx >= (int)tabs_.size()) return;
    if (tabs_[idx].dirty) {
        // Show confirmation — for now, just close (ImGui popup handled in render)
    }
    delete tabs_[idx].editor;
    tabs_.erase(tabs_.begin() + idx);
    if (tabs_.empty()) {
        new_tab();
    } else if (active_tab_ >= (int)tabs_.size()) {
        active_tab_ = (int)tabs_.size() - 1;
    }
}

void EditorApp::close_window() {
    bool any_unsaved = false;
    for (auto& t : tabs_) {
        if (t.dirty) { any_unsaved = true; break; }
    }
    if (!any_unsaved) {
        running_ = false;
        return;
    }
    // For now, just close
    running_ = false;
}

// ============================================================================
// Recent files
// ============================================================================
void EditorApp::add_recent_file(const std::string& path) {
    // Remove if already exists
    settings_.recent_files.erase(
        std::remove(settings_.recent_files.begin(), settings_.recent_files.end(), path),
        settings_.recent_files.end());
    // Add to front
    settings_.recent_files.insert(settings_.recent_files.begin(), path);
    // Keep only last 10
    if (settings_.recent_files.size() > 10) {
        settings_.recent_files.resize(10);
    }
}

// ============================================================================
// Search
// ============================================================================
void EditorApp::find_next() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    TextEditor* ed = get_active_editor();
    std::string text = ed->GetText();
    std::string search = find_text_;
    if (!find_match_case_) {
        // Case-insensitive search
        std::string lower_text = text, lower_search = search;
        std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
        std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(), ::tolower);

        auto pos = ed->GetCursorPosition();
        // Simple: search from current cursor position
        for (int line = 0; line < pos.mLine; line++) {
            // Approximate
        }
        // Use TextEditor's built-in search if available, otherwise simple
        auto found = lower_text.find(lower_search, 0);
        if (found != std::string::npos) {
            // Calculate line/column from character position
            int line = 0, col = 0;
            for (size_t i = 0; i < found && i < text.size(); i++) {
                if (text[i] == '\n') { line++; col = 0; }
                else col++;
            }
            ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        }
    } else {
        auto found = text.find(search, 0);
        if (found != std::string::npos) {
            int line = 0, col = 0;
            for (size_t i = 0; i < found && i < text.size(); i++) {
                if (text[i] == '\n') { line++; col = 0; }
                else col++;
            }
            ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        }
    }
}

void EditorApp::find_prev() {
    // Similar to find_next but search backwards
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    TextEditor* ed = get_active_editor();
    std::string text = ed->GetText();
    std::string search = find_text_;
    if (!find_match_case_) {
        std::string lower_text = text, lower_search = search;
        std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
        std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(), ::tolower);
        auto found = lower_text.rfind(lower_search);
        if (found != std::string::npos) {
            int line = 0, col = 0;
            for (size_t i = 0; i < found && i < text.size(); i++) {
                if (text[i] == '\n') { line++; col = 0; }
                else col++;
            }
            ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        }
    } else {
        auto found = text.rfind(search);
        if (found != std::string::npos) {
            int line = 0, col = 0;
            for (size_t i = 0; i < found && i < text.size(); i++) {
                if (text[i] == '\n') { line++; col = 0; }
                else col++;
            }
            ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        }
    }
}

void EditorApp::replace_one() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    TextEditor* ed = get_active_editor();
    std::string text = ed->GetText();
    std::string search = find_text_;
    if (!find_match_case_) {
        std::string lt = text, ls = search;
        std::transform(lt.begin(), lt.end(), lt.begin(), ::tolower);
        std::transform(ls.begin(), ls.end(), ls.begin(), ::tolower);
        auto pos = lt.find(ls);
        if (pos != std::string::npos) {
            text.replace(pos, search.size(), replace_text_);
            ed->SetText(text);
            tabs_[active_tab_].dirty = true;
        }
    } else {
        auto pos = text.find(search);
        if (pos != std::string::npos) {
            text.replace(pos, search.size(), replace_text_);
            ed->SetText(text);
            tabs_[active_tab_].dirty = true;
        }
    }
}

void EditorApp::replace_all() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    TextEditor* ed = get_active_editor();
    std::string text = ed->GetText();
    std::string search = find_text_;
    std::string repl = replace_text_;
    size_t count = 0;

    if (!find_match_case_) {
        std::string lt = text, ls = search;
        std::transform(lt.begin(), lt.end(), lt.begin(), ::tolower);
        std::transform(ls.begin(), ls.end(), ls.begin(), ::tolower);
        size_t pos = 0;
        while ((pos = lt.find(ls, pos)) != std::string::npos) {
            text.replace(pos, search.size(), repl);
            lt.replace(pos, ls.size(), std::string(repl.size(), ' '));  // Avoid re-matching
            pos += repl.size();
            count++;
        }
    } else {
        size_t pos = 0;
        while ((pos = text.find(search, pos)) != std::string::npos) {
            text.replace(pos, search.size(), repl);
            pos += repl.size();
            count++;
        }
    }

    ed->SetText(text);
    tabs_[active_tab_].dirty = true;
}

// ============================================================================
// View
// ============================================================================
void EditorApp::zoom_in() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    tabs_[active_tab_].zoom_pct = (std::min)(200, tabs_[active_tab_].zoom_pct + 10);
    apply_zoom(active_tab_);
}

void EditorApp::zoom_out() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    tabs_[active_tab_].zoom_pct = (std::max)(50, tabs_[active_tab_].zoom_pct - 10);
    apply_zoom(active_tab_);
}

void EditorApp::zoom_reset() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    tabs_[active_tab_].zoom_pct = 100;
    apply_zoom(active_tab_);
}

void EditorApp::apply_zoom(int tab_idx) {
    if (tab_idx < 0 || tab_idx >= (int)tabs_.size()) return;
}

void EditorApp::terminal_zoom_in() {
    terminal_zoom_pct_ = (std::min)(200, terminal_zoom_pct_ + 10);
}

void EditorApp::terminal_zoom_out() {
    terminal_zoom_pct_ = (std::max)(50, terminal_zoom_pct_ - 10);
}

void EditorApp::terminal_zoom_reset() {
    terminal_zoom_pct_ = 100;
}

void EditorApp::toggle_status_bar() {
    settings_.show_status_bar = !settings_.show_status_bar;
}

void EditorApp::toggle_explorer() {
    show_file_tree_ = !show_file_tree_;
}

void EditorApp::toggle_word_wrap() {
    settings_.word_wrap = !settings_.word_wrap;
    // Apply to all tabs
    for (auto& tab : tabs_) {
        tab.editor->SetImGuiChildIgnored(settings_.word_wrap);
    }
}

void EditorApp::toggle_line_numbers() {
    settings_.show_line_numbers = !settings_.show_line_numbers;
    // Note: TextEditor always shows line numbers in this version
}

void EditorApp::toggle_spaces() {
    settings_.show_spaces = !settings_.show_spaces;
    for (auto& tab : tabs_) {
        tab.editor->SetShowWhitespaces(settings_.show_spaces);
    }
}

void EditorApp::toggle_minimap() {
    settings_.show_minimap = !settings_.show_minimap;
}

void EditorApp::toggle_theme() {
    settings_.dark_theme = !settings_.dark_theme;
    apply_theme(settings_.dark_theme);
    
    // Apply theme to all tabs immediately
    for (auto& tab : tabs_) {
        if (settings_.dark_theme) {
            tab.editor->SetPalette(TextEditor::GetDarkPalette());
        } else {
            tab.editor->SetPalette(TextEditor::GetLightPalette());
        }
    }
}

void EditorApp::set_tab_size(int size) {
    if (size < 1 || size > 16) return;
    settings_.tab_size = size;
    
    // Apply to all tabs
    for (auto& tab : tabs_) {
        tab.editor->SetTabSize(size);
    }
}

// ============================================================================
// Bookmarks
// ============================================================================
void EditorApp::toggle_bookmark(int line) {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& bookmarks = tabs_[active_tab_].bookmarks;
    auto it = std::find(bookmarks.begin(), bookmarks.end(), line);
    if (it != bookmarks.end()) {
        bookmarks.erase(it);
    } else {
        bookmarks.push_back(line);
        std::sort(bookmarks.begin(), bookmarks.end());
    }
}

void EditorApp::next_bookmark() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& bookmarks = tabs_[active_tab_].bookmarks;
    if (bookmarks.empty()) return;
    auto pos = get_active_editor()->GetCursorPosition().mLine;
    auto it = std::upper_bound(bookmarks.begin(), bookmarks.end(), pos);
    if (it != bookmarks.end()) {
        get_active_editor()->SetCursorPosition(TextEditor::Coordinates(*it, 0));
    } else if (!bookmarks.empty()) {
        get_active_editor()->SetCursorPosition(TextEditor::Coordinates(bookmarks[0], 0));
    }
}

void EditorApp::prev_bookmark() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& bookmarks = tabs_[active_tab_].bookmarks;
    if (bookmarks.empty()) return;
    auto pos = get_active_editor()->GetCursorPosition().mLine;
    auto it = std::lower_bound(bookmarks.begin(), bookmarks.end(), pos);
    if (it != bookmarks.begin()) {
        --it;
        get_active_editor()->SetCursorPosition(TextEditor::Coordinates(*it, 0));
    } else {
        get_active_editor()->SetCursorPosition(TextEditor::Coordinates(bookmarks.back(), 0));
    }
}

void EditorApp::clear_bookmarks() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    tabs_[active_tab_].bookmarks.clear();
}

// ============================================================================
// Code Folding
// ============================================================================
void EditorApp::toggle_fold(int line) {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& folds = tabs_[active_tab_].folds;
    
    // Check if there's already a fold starting at this line
    for (auto it = folds.begin(); it != folds.end(); ++it) {
        if (it->first == line) {
            folds.erase(it);
            return;
        }
    }
    
    // Find matching closing brace
    auto& editor = *get_active_editor();
    int total_lines = editor.GetTotalLines();
    int brace_count = 0;
    int end_line = line;
    
    for (int i = line; i < total_lines; i++) {
        std::string text = editor.GetTextLines()[i];
        for (char c : text) {
            if (c == '{') brace_count++;
            else if (c == '}') brace_count--;
        }
        if (brace_count <= 0 && i > line) {
            end_line = i;
            break;
        }
    }
    
    if (end_line > line) {
        folds.push_back({line, end_line});
    }
}

void EditorApp::fold_all() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    tab.folds.clear();
    
    auto& editor = *tab.editor;
    auto& lines = editor.GetTextLines();
    int brace_count = 0;
    int fold_start = -1;
    
    for (int i = 0; i < (int)lines.size(); i++) {
        for (char c : lines[i]) {
            if (c == '{') {
                if (brace_count == 0) fold_start = i;
                brace_count++;
            } else if (c == '}') {
                brace_count--;
                if (brace_count == 0 && fold_start >= 0 && i > fold_start) {
                    tab.folds.push_back({fold_start, i});
                    fold_start = -1;
                }
            }
        }
    }
}

void EditorApp::unfold_all() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    tabs_[active_tab_].folds.clear();
}

// ============================================================================
// Change History
// ============================================================================
void EditorApp::update_change_history() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    int current_lines = tab.editor->GetTotalLines();
    
    if (tab.last_saved_line_count == 0) {
        tab.last_saved_line_count = current_lines;
        return;
    }
    
    if (current_lines > tab.last_saved_line_count) {
        for (int i = tab.last_saved_line_count; i < current_lines; i++) {
            tab.changed_lines.push_back(i);
        }
    }
    tab.last_saved_line_count = current_lines;
}

void EditorApp::clear_change_history() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    tabs_[active_tab_].changed_lines.clear();
}

void EditorApp::load_fonts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Clear existing fonts first
    io.Fonts->Clear();
    
    // Auto-detect DPI and adjust font size
    float dpi_scale = 1.0f;
#if defined(_WIN32)
    HDC hdc = GetDC(NULL);
    if (hdc) {
        dpi_scale = GetDeviceCaps(hdc, 88) / 96.0f; // 88 = LOGPIXELSX
        ReleaseDC(NULL, hdc);
    }
#endif
    // Apply DPI scaling to default font size
    int dpi_adjusted_size = (int)(settings_.font_size * dpi_scale);
    dpi_adjusted_size = (std::max)(8, (std::min)(dpi_adjusted_size, 72));
    float font_size = (float)dpi_adjusted_size;
    
    ImFont* font = nullptr;
    
#if defined(_WIN32)
    // Try to load user-selected font - scan actual files in C:/Windows/Fonts/
    if (!settings_.font_name.empty()) {
        std::vector<std::string> exts = {".ttf", ".otf", ".ttc"};
        std::string name = settings_.font_name;
        
        for (const auto& ext : exts) {
            std::string path = std::string("C:\\Windows\\Fonts\\") + name + ext;
            // Check if file exists before trying to load
            if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                font = io.Fonts->AddFontFromFileTTF(path.c_str(), font_size);
                if (font) break;
            }
        }
        // If couldn't load, clear the font name
        if (!font) {
            settings_.font_name = "";
        }
    }
#else
    (void)settings_; // unused on non-Windows
#endif
    
    // Fallback to built-in default (not system font)
    if (!font) {
        ImFontConfig* config = new ImFontConfig();
        config->SizePixels = font_size;
        font = io.Fonts->AddFontDefault(config);
        delete config;
    }
    
    font_size_temp_ = settings_.font_size;
    font_name_temp_ = settings_.font_name;
    
    float scale = (float)settings_.font_size / 16.0f;
    io.FontGlobalScale = scale;
}

void EditorApp::rebuild_fonts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Update font size
    settings_.font_size = font_size_temp_;
    
    // Clear and rebuild fonts
    io.Fonts->Clear();
    
    // Auto-detect DPI and adjust font size
    float dpi_scale = 1.0f;
#if defined(_WIN32)
    HDC hdc = GetDC(NULL);
    if (hdc) {
        dpi_scale = GetDeviceCaps(hdc, 88) / 96.0f; // 88 = LOGPIXELSX
        ReleaseDC(NULL, hdc);
    }
#endif
    int dpi_adjusted_size = (int)(settings_.font_size * dpi_scale);
    dpi_adjusted_size = (std::max)(8, (std::min)(dpi_adjusted_size, 72));
    float font_size = (float)dpi_adjusted_size;
    
    ImFont* font = nullptr;
    
#if defined(_WIN32)
    // Try to load user-selected font - scan actual files in C:/Windows/Fonts/
    if (!font_name_temp_.empty()) {
        std::vector<std::string> exts = {".ttf", ".otf", ".ttc"};
        std::string name = font_name_temp_;
        
        for (const auto& ext : exts) {
            std::string path = std::string("C:\\Windows\\Fonts\\") + name + ext;
            if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                font = io.Fonts->AddFontFromFileTTF(path.c_str(), font_size);
                if (font) {
                    settings_.font_name = font_name_temp_;
                    break;
                }
            }
        }
    }
#else
    (void)font_name_temp_;
#endif
    
    // Fallback to built-in default (not system font)
    if (!font) {
        ImFontConfig* config = new ImFontConfig();
        config->SizePixels = font_size;
        font = io.Fonts->AddFontDefault(config);
        delete config;
        font_name_temp_ = "";
        settings_.font_name = "";
    }
    
    font_size_temp_ = settings_.font_size;
    font_name_temp_ = settings_.font_name;
    
    float scale = (float)settings_.font_size / 16.0f;
    io.FontGlobalScale = scale;
}

// ============================================================================
// Vim Mode
// ============================================================================
const char* EditorApp::get_vim_mode_str() const {
    switch (vim_mode_) {
        case VimMode::Normal: return "NORMAL";
        case VimMode::Insert: return "INSERT";
        case VimMode::Visual: return "VISUAL";
        case VimMode::VisualLine: return "VISUAL LINE";
        case VimMode::OperatorPending: return "OP PENDING";
        case VimMode::Command: return ":";
        default: return "";
    }
}

void EditorApp::handle_vim_key(int key, int mods) {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    TextEditor* ed = get_active_editor();
    if (!ed) return;
    auto pos = ed->GetCursorPosition();
    int line = pos.mLine;
    int col = pos.mColumn;
    auto& lines = ed->GetTextLines();
    
    // Escape cancels everything
    if (key == ImGuiKey_Escape) {
        vim_mode_ = VimMode::Normal;
        vim_operator_ = 0;
        vim_count_ = 0;
        vim_key_buffer_.clear();
        return;
    }
    
    // In insert mode, only Escape matters
    if (vim_mode_ == VimMode::Insert) {
        return;
    }
    
    // Map key to char
    char c = 0;
    if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
        c = (char)('a' + (key - ImGuiKey_A));
    } else if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
        c = (char)('0' + (key - ImGuiKey_0));
    } else if (key == ImGuiKey_Space) c = ' ';
    else if (key == ImGuiKey_Enter) c = '\n';
    else if (key == ImGuiKey_Backspace) c = 8;
    else if (key == ImGuiKey_Tab) c = 9;
    else if (key == ImGuiKey_Period) c = '.';
    else if (key == ImGuiKey_Minus) c = '-';
    
    if (!c) return;
    
    // Handle count prefix (digits)
    if (c >= '0' && c <= '9' && vim_key_buffer_.empty()) {
        if (c == '0' && vim_count_ == 0) {
            // 0 is a motion, not a count
        } else {
            vim_count_ = vim_count_ * 10 + (c - '0');
            return;
        }
    }
    
    vim_key_buffer_ += c;
    std::string& kb = vim_key_buffer_;
    int count = vim_count_ > 0 ? vim_count_ : 1;
    
    // Single key commands
    if (kb.size() == 1) {
        switch (kb[0]) {
            case 'h':
                ed->SetCursorPosition(TextEditor::Coordinates(line, (std::max)(0, col - count)));
                break;
            case 'j':
                ed->SetCursorPosition(TextEditor::Coordinates((std::min)(ed->GetTotalLines() - 1, line + count), col));
                break;
            case 'k':
                ed->SetCursorPosition(TextEditor::Coordinates((std::max)(0, line - count), col));
                break;
            case 'l':
                if (line < (int)lines.size() && col < (int)lines[line].size()) {
                    ed->SetCursorPosition(TextEditor::Coordinates(line, col + count));
                }
                break;
            case '0':
                ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                break;
            case '^':
                if (line < (int)lines.size()) {
                    std::string& text = lines[line];
                    int start = 0;
                    while (start < (int)text.size() && (text[start] == ' ' || text[start] == '\t')) start++;
                    ed->SetCursorPosition(TextEditor::Coordinates(line, start));
                }
                break;
            case '$':
                if (line < (int)lines.size()) {
                    ed->SetCursorPosition(TextEditor::Coordinates(line, (int)lines[line].size()));
                }
                break;
            case 'i':
                vim_mode_ = VimMode::Insert;
                break;
            case 'I':
                ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                vim_mode_ = VimMode::Insert;
                break;
            case 'a':
                if (line < (int)lines.size() && col < (int)lines[line].size()) {
                    ed->SetCursorPosition(TextEditor::Coordinates(line, col + 1));
                }
                vim_mode_ = VimMode::Insert;
                break;
            case 'A':
                if (line < (int)lines.size()) {
                    ed->SetCursorPosition(TextEditor::Coordinates(line, (int)lines[line].size()));
                }
                vim_mode_ = VimMode::Insert;
                break;
            case 'o':
                ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                ed->InsertText("\n");
                vim_mode_ = VimMode::Insert;
                break;
            case 'O':
                ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                ed->InsertText("\n");
                if (line > 0) ed->SetCursorPosition(TextEditor::Coordinates(line - 1, 0));
                vim_mode_ = VimMode::Insert;
                break;
            case 'x':
                for (int i = 0; i < count; i++) {
                    if (line < (int)lines.size() && col < (int)lines[line].size()) {
                        ed->Delete();
                    }
                }
                break;
            case 'w':
                while (count-- > 0) {
                    int next_col = col;
                    if (line < (int)lines.size()) {
                        std::string& text = lines[line];
                        while (next_col < (int)text.size() && (text[next_col] == ' ' || text[next_col] == '\t')) next_col++;
                        while (next_col < (int)text.size()) {
                            if ((text[next_col] >= 'a' && text[next_col] <= 'z') ||
                                (text[next_col] >= 'A' && text[next_col] <= 'Z') ||
                                (text[next_col] >= '0' && text[next_col] <= '9') ||
                                text[next_col] == '_') break;
                            next_col++;
                        }
                        if (next_col < (int)text.size()) {
                            while (next_col < (int)text.size()) {
                                if ((text[next_col] >= 'a' && text[next_col] <= 'z') ||
                                    (text[next_col] >= 'A' && text[next_col] <= 'Z') ||
                                    (text[next_col] >= '0' && text[next_col] <= '9') ||
                                    text[next_col] == '_') {
                                    next_col++;
                                } else {
                                    break;
                                }
                            }
                        }
                        if (next_col >= (int)text.size() && line < ed->GetTotalLines() - 1) {
                            line++;
                            col = 0;
                            if (line < (int)lines.size()) text = lines[line];
                            next_col = 0;
                            while (next_col < (int)text.size() && (text[next_col] == ' ' || text[next_col] == '\t')) next_col++;
                        } else {
                            col = next_col;
                        }
                        ed->SetCursorPosition(TextEditor::Coordinates(line, col));
                    }
                }
                break;
            case 'b':
                while (count-- > 0) {
                    if (line < (int)lines.size()) {
                        std::string& text = lines[line];
                        int prev_col = col;
                        while (prev_col > 0) {
                            char c = text[prev_col - 1];
                            bool is_word = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                        (c >= '0' && c <= '9') || c == '_';
                            if (!is_word) break;
                            prev_col--;
                        }
                        while (prev_col > 0) {
                            char c = text[prev_col - 1];
                            bool is_word = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                        (c >= '0' && c <= '9') || c == '_';
                            if (is_word) prev_col--;
                            else break;
                        }
                        col = prev_col;
                        ed->SetCursorPosition(TextEditor::Coordinates(line, col));
                    }
                }
                break;
            case 'e':
                while (count-- > 0) {
                    if (line < (int)lines.size()) {
                        std::string& text = lines[line];
                        int end_col = col;
                        while (end_col < (int)text.size()) {
                            char c = text[end_col];
                            bool is_word = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                        (c >= '0' && c <= '9') || c == '_';
                            if (!is_word) break;
                            end_col++;
                        }
                        if (end_col < (int)text.size()) {
                            while (end_col < (int)text.size()) {
                                char c = text[end_col];
                                bool is_word = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                            (c >= '0' && c <= '9') || c == '_';
                                if (is_word) end_col++;
                                else break;
                            }
                        }
                        col = end_col;
                        ed->SetCursorPosition(TextEditor::Coordinates(line, col));
                    }
                }
                break;
            case 'X':
                for (int i = 0; i < count; i++) {
                    if (col > 0) {
                        ed->SetCursorPosition(TextEditor::Coordinates(line, col - 1));
                        ed->Delete();
                        col--;
                    }
                }
                break;
            case 'd':
                vim_mode_ = VimMode::OperatorPending;
                vim_operator_ = 'd';
                return;
            case 'y':
                vim_mode_ = VimMode::OperatorPending;
                vim_operator_ = 'y';
                return;
            case 'c':
                vim_mode_ = VimMode::OperatorPending;
                vim_operator_ = 'c';
                return;
            case 'v':
                vim_mode_ = VimMode::Visual;
                return;
            case 'V':
                vim_mode_ = VimMode::VisualLine;
                return;
            case 'u':
                for (int i = 0; i < count; i++) ed->Undo();
                break;
            case 'p':
                ed->Paste();
                break;
            case 'P':
                ed->SetCursorPosition(TextEditor::Coordinates(line, (std::max)(0, col - 1)));
                ed->Paste();
                break;
            case '~':
                if (line < (int)lines.size() && col < (int)lines[line].size()) {
                    char ch = lines[line][col];
                    if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
                    else if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
                    lines[line][col] = ch;
                    tabs_[active_tab_].dirty = true;
                }
                break;
            case 'G': {
                int target = count > 0 ? count - 1 : ed->GetTotalLines() - 1;
                target = (std::max)(0, (std::min)(target, ed->GetTotalLines() - 1));
                ed->SetCursorPosition(TextEditor::Coordinates(target, 0));
                break;
            }
            case 'g':
                vim_mode_ = VimMode::OperatorPending;
                vim_operator_ = 'g';
                return;
            case '.':
                break;
            default:
                break;
        }
        
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    
    // Multi-key commands
    if (kb == "gg") {
        int target = vim_count_ > 0 ? vim_count_ - 1 : 0;
        ed->SetCursorPosition(TextEditor::Coordinates(target, 0));
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    
    if (kb == "dd") {
        ed->SetSelectionStart(TextEditor::Coordinates(line, 0));
        if (line < (int)lines.size()) {
            ed->SetSelectionEnd(TextEditor::Coordinates(line, (int)lines[line].size()));
        }
        ed->Cut();
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    
    if (kb == "yy") {
        ed->SetSelectionStart(TextEditor::Coordinates(line, 0));
        if (line < (int)lines.size()) {
            ed->SetSelectionEnd(TextEditor::Coordinates(line, (int)lines[line].size()));
        }
        ed->Copy();
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    
    if (kb == "cc") {
        ed->SetSelectionStart(TextEditor::Coordinates(line, 0));
        if (line < (int)lines.size()) {
            ed->SetSelectionEnd(TextEditor::Coordinates(line, (int)lines[line].size()));
        }
        ed->Delete();
        vim_mode_ = VimMode::Insert;
        vim_key_buffer_.clear();
        vim_count_ = 0;
        return;
    }
    
    // Operator pending
    if (vim_mode_ == VimMode::OperatorPending && kb.size() >= 1) {
        char op = vim_operator_;
        char motion = kb[0];
        
        if (op == 'd') {
            if (motion == 'd') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, 0));
                if (line < (int)lines.size()) {
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, (int)lines[line].size()));
                }
                ed->Delete();
            }
        } else if (op == 'y') {
            if (motion == 'y') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, 0));
                if (line < (int)lines.size()) {
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, (int)lines[line].size()));
                }
                ed->Copy();
            }
        } else if (op == 'c') {
            if (motion == 'c') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, 0));
                if (line < (int)lines.size()) {
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, (int)lines[line].size()));
                }
                ed->Delete();
                vim_mode_ = VimMode::Insert;
            }
        }
        
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_operator_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    
    vim_key_buffer_.clear();
    vim_count_ = 0;
    vim_mode_ = VimMode::Normal;
}

bool EditorApp::execute_vim_command(const std::string& cmd) {
    if (cmd.empty()) return false;
    
    std::string command = cmd;
    if (command[0] == ':') command = command.substr(1);
    
    if (command == "version" || command == "version\n") {
        vim_command_buffer_ = get_version();
        vim_mode_ = VimMode::Command;
        ImGui::OpenPopup("##CommandLine");
        return true;
    }
    
    // Handle :set commands
    if (command.substr(0, 4) == "set ") {
        std::string opt = command.substr(4);
        bool toggle = (opt.find("no") != 0);
        std::string opt_name = toggle ? opt : opt.substr(2);
        
        if (opt_name == "nu" || opt_name == "number") {
            settings_.show_line_numbers = toggle;
            vim_command_buffer_ = opt_name + " " + std::string(toggle ? "enabled" : "disabled");
        } else if (opt_name == "hl" || opt_name == "hlsearch") {
            settings_.highlight_line = toggle ? 1 : 0;
            vim_command_buffer_ = "hlsearch " + std::string(toggle ? "enabled" : "disabled");
        } else if (opt_name == "list") {
            settings_.show_spaces = toggle;
            vim_command_buffer_ = "list " + std::string(toggle ? "enabled" : "disabled");
        } else if (opt_name == "wrap") {
            settings_.word_wrap = toggle;
            vim_command_buffer_ = "wrap " + std::string(toggle ? "enabled" : "disabled");
        } else if (opt_name == "minimap") {
            settings_.show_minimap = toggle;
            vim_command_buffer_ = "minimap " + std::string(toggle ? "enabled" : "disabled");
        } else if (opt_name == "ws" || opt_name == "whitespace") {
            vim_command_buffer_ = "whitespace " + std::string(settings_.show_spaces ? "visible" : "hidden");
        } else if (opt_name == "tab") {
            vim_command_buffer_ = "tab=" + std::to_string(settings_.tab_size);
        } else if (opt_name == "all") {
            vim_command_buffer_ = "number: " + std::to_string(settings_.show_line_numbers) + 
                              "\nhlsearch: " + std::to_string(settings_.highlight_line > 0) +
                              "\nlist: " + std::to_string(settings_.show_spaces) +
                              "\nwrap: " + std::to_string(settings_.word_wrap) +
                              "\nminimap: " + std::to_string(settings_.show_minimap) +
                              "\ntab: " + std::to_string(settings_.tab_size);
        } else {
            vim_command_buffer_ = "Unknown option: " + opt_name;
        }
        vim_mode_ = VimMode::Command;
        ImGui::OpenPopup("##CommandLine");
        return true;
    }
    
    if (command == "d" || command == "diag" || command == "diagnostics") {
        char buf[512];
        sprintf(buf, "pCode Editor Diagnostics:\nVersion: %s\nTabs Open: %zu\nTabs Dirty:", get_version().c_str(), tabs_.size());
        for (size_t i = 0; i < tabs_.size(); i++) {
            if (tabs_[i].dirty) sprintf(buf + strlen(buf), " %zu", i);
        }
        vim_command_buffer_ = buf;
        vim_mode_ = VimMode::Command;
        ImGui::OpenPopup("##CommandLine");
        return true;
    }
    
    if (command == "q") {
        close_tab(active_tab_);
        return true;
    }
    if (command == "q!") {
        close_tab(active_tab_);
        return true;
    }
    if (command == "w") {
        save_tab(active_tab_);
        return true;
    }
    if (command == "wq") {
        save_tab(active_tab_);
        close_tab(active_tab_);
        return true;
    }
    if (command == "e") {
        open_file("");
        return true;
    }
    if (command == "e!") {
        open_file("");
        return true;
    }
    if ((command.substr(0, 2) == "e " || command.substr(0, 3) == "e! ") && command.length() > 2) {
        std::string path = command.substr(2);
        if (path.empty()) {
            open_file("");
            return true;
        }
        if (!std::filesystem::exists(path)) {
            // Try relative to explorer tree directory (last_open_dir)
            std::string explorer_dir = settings_.last_open_dir.empty() ? "." : settings_.last_open_dir;
            std::string full_path = explorer_dir + "\\" + path;
            if (std::filesystem::exists(full_path)) {
                open_file(full_path);
            } else {
                // Fall back to current working directory
                std::string cwd = std::filesystem::current_path().string();
                full_path = cwd + "\\" + path;
                if (std::filesystem::exists(full_path)) {
                    open_file(full_path);
                } else {
                    fprintf(stderr, "Failed to open: %s\n", path.c_str());
                }
            }
        } else {
            open_file(path);
        }
        return true;
    }
    if (command == "new") {
        new_tab();
        return true;
    }
    if (command == "tabnew") {
        new_tab();
        return true;
    }
    if (command == "tabe") {
        open_file("");
        return true;
    }
    if (command.substr(0, 4) == "tabe" && command.length() > 5) {
        std::string path = command.substr(5);
        open_file(path);
        return true;
    }
    if (command == "N" || command == "n") {
        new_window();
        return true;
    }
    // Handle :sp [file] - split horizontally and optionally open file
    if (command.substr(0, 3) == "sp ") {
        std::string path = command.substr(3);
        if (path.empty()) {
            split_horizontal();
        } else {
            split_horizontal();
            open_file_split(path);
        }
        return true;
    }
    if (command == "sp") {
        split_horizontal();
        return true;
    }
    
    // Handle :vsp [file] - split vertically and optionally open file
    if (command.substr(0, 4) == "vsp ") {
        std::string path = command.substr(4);
        if (path.empty()) {
            split_vertical();
        } else {
            split_vertical();
            open_file_split(path);
        }
        return true;
    }
    if (command == "vsp") {
        split_vertical();
        return true;
    }
    
    // Handle :vs - alias for :vsp
    if (command.substr(0, 3) == "vs ") {
        std::string path = command.substr(3);
        if (path.empty()) {
            split_vertical();
        } else {
            split_vertical();
            open_file_split(path);
        }
        return true;
    }
    if (command == "vs") {
        split_vertical();
        return true;
    }
    if (command == "sh" || command == "shell" || command == "term") {
        show_terminal_ = true;
        if (!terminal_process_) start_terminal();
        return true;
    }
    if (command == "on" || command == "only") {
        return true;
    }
    
    return false;
}

// ============================================================================
// Helpers
// ============================================================================
std::string EditorApp::get_file_encoding(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "UTF-8";
    unsigned char bom[4];
    f.read((char*)bom, 4);
    if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) return "UTF-8 BOM";
    if (bom[0] == 0xFF && bom[1] == 0xFE) return "UTF-16 LE";
    if (bom[0] == 0xFE && bom[1] == 0xFF) return "UTF-16 BE";
    return "UTF-8";
}

std::string EditorApp::get_line_ending(const std::string& content) {
    if (content.find("\r\n") != std::string::npos) return "CRLF";
    if (content.find("\r") != std::string::npos) return "CR";
    return "LF";
}

size_t EditorApp::get_file_size(const std::string& path) {
    try { return std::filesystem::file_size(path); } catch (...) { return 0; }
}

// ============================================================================
// Render
// ============================================================================
void EditorApp::render() {
    // Global keyboard shortcuts
    ImGuiIO& io = ImGui::GetIO();
    
    // Reset vim input skip flag at start of each frame
    skip_texteditor_input_ = false;
    
    // Handle Vim mode keys first (skip when terminal input is active or editor not focused)
    // Only handle vim keys when in Normal mode and editor is focused
    if (vim_mode_ == VimMode::Normal && !terminal_input_active_) {
        bool vim_key_handled = false;
        for (int key = ImGuiKey_A; key <= ImGuiKey_Z; key++) {
            if (ImGui::IsKeyPressed((ImGuiKey)key)) { handle_vim_key(key, 0); vim_key_handled = true; break; }
        }
        if (!vim_key_handled) {
            for (int key = ImGuiKey_0; key <= ImGuiKey_9; key++) {
                if (ImGui::IsKeyPressed((ImGuiKey)key)) { handle_vim_key(key, 0); vim_key_handled = true; break; }
            }
        }
        if (!vim_key_handled && ImGui::IsKeyPressed(ImGuiKey_Space)) { handle_vim_key(ImGuiKey_Space, 0); vim_key_handled = true; }
        if (!vim_key_handled && ImGui::IsKeyPressed(ImGuiKey_Enter)) { handle_vim_key(ImGuiKey_Enter, 0); vim_key_handled = true; }
        if (!vim_key_handled && ImGui::IsKeyPressed(ImGuiKey_Escape)) { vim_mode_ = VimMode::Normal; vim_key_buffer_.clear(); vim_count_ = 0; return; }
        if (!vim_key_handled && ImGui::IsKeyPressed(ImGuiKey_Backspace)) { handle_vim_key(ImGuiKey_Backspace, 0); vim_key_handled = true; }
        if (!vim_key_handled && ImGui::IsKeyPressed(ImGuiKey_Tab)) { handle_vim_key(ImGuiKey_Tab, 0); vim_key_handled = true; }
        if (!vim_key_handled && ImGui::IsKeyPressed(ImGuiKey_Minus)) { handle_vim_key(ImGuiKey_Minus, 0); vim_key_handled = true; }
        if (!vim_key_handled && ImGui::IsKeyPressed(ImGuiKey_Period)) { handle_vim_key(ImGuiKey_Period, 0); vim_key_handled = true; }
        if (!vim_key_handled && ImGui::IsKeyPressed(ImGuiKey_Semicolon) && io.KeyShift) { vim_mode_ = VimMode::Command; vim_command_buffer_ = ":"; return; }
        
        // Handle Ctrl+W for split navigation (Vim style)
        // First check Ctrl+W was just pressed
        static bool ctrl_w_pressed = false;
        if (ImGui::IsKeyPressed(ImGuiKey_W) && io.KeyCtrl) { ctrl_w_pressed = true; }
        // Then check for second key while Ctrl is still held or was held
        if (ctrl_w_pressed) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) { next_split(); ctrl_w_pressed = false; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_H)) { prev_split(); ctrl_w_pressed = false; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_J)) { next_split(); ctrl_w_pressed = false; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_K)) { prev_split(); ctrl_w_pressed = false; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_L)) { next_split(); ctrl_w_pressed = false; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_P)) { prev_split(); ctrl_w_pressed = false; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_C)) { close_split(); ctrl_w_pressed = false; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_S)) { split_horizontal(); ctrl_w_pressed = false; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_V)) { split_vertical(); ctrl_w_pressed = false; return; }
            // Clear ctrl_w if no second key was pressed
            if (!io.KeyCtrl) ctrl_w_pressed = false;
        }
        
        // After vim key handling, clear keys to prevent TextEditor from receiving them
        if (vim_key_handled) {
            // Clear the keys we just handled so TextEditor doesn't see them
            io.AddKeyEvent(ImGuiKey_J, false);
            io.AddKeyEvent(ImGuiKey_K, false);
            io.AddKeyEvent(ImGuiKey_H, false);
            io.AddKeyEvent(ImGuiKey_L, false);
            for (int key = ImGuiKey_A; key <= ImGuiKey_Z; key++) {
                io.AddKeyEvent((ImGuiKey)key, false);
            }
            for (int key = ImGuiKey_0; key <= ImGuiKey_9; key++) {
                io.AddKeyEvent((ImGuiKey)key, false);
            }
            io.AddKeyEvent(ImGuiKey_Space, false);
            io.AddKeyEvent(ImGuiKey_Backspace, false);
            io.AddKeyEvent(ImGuiKey_Tab, false);
        }
    }
    
    // Command mode - don't process vim keys or shortcuts, let status bar handle input
    if (vim_mode_ != VimMode::Command) {
        if (io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_O)) { open_file(""); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_S)) { save_tab(active_tab_); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_N)) { new_tab(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_W)) { close_tab(active_tab_); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_F)) { show_find_ = true; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_H)) { show_replace_ = true; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_G)) { show_goto_ = true; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_A)) { 
                if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                    get_active_editor()->SelectAll(); 
                }
                return; 
            }
        }
        if (io.KeyCtrl && io.KeyShift && !io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_N)) { new_window(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_S)) { save_tab_as(active_tab_); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_W)) { close_window(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_P)) { show_cmd_palette_ = true; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_H)) { split_horizontal(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_V)) { split_vertical(); return; }
        }
        if (io.KeyCtrl && io.KeyAlt && !io.KeyShift) {
            if (ImGui::IsKeyPressed(ImGuiKey_S)) { save_all(); return; }
        }
        if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
            if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F3)) { prev_bookmark(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_F3)) { find_next(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_F4)) { next_bookmark(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_F2)) { 
                if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                    auto pos = get_active_editor()->GetCursorPosition();
                    toggle_bookmark(pos.mLine);
                }
                return; 
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
                if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                    auto now = std::chrono::system_clock::now();
                    auto t = std::chrono::system_clock::to_time_t(now);
                    char buf[64];
                    strftime(buf, sizeof(buf), "%H:%M %Y-%m-%d", localtime(&t));
                    get_active_editor()->InsertText(buf);
                    tabs_[active_tab_].dirty = true;
                }
                return;
            }
        }
        if (io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) { zoom_in(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) { zoom_out(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_0)) { zoom_reset(); return; }
        }
    }

// Main window - just sidebar
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("pcode-editor", nullptr, flags);
    ImGui::PopStyleVar(3);

    render_sidebar();

    ImGui::End();

    // Editor as independent floating window - fully movable, with menu inside
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 80, viewport->Pos.y + 80), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_MenuBar)) {
        render_menu_bar();
        render_editor_area();
        ImGui::End();
    }

    // Dialogs
    if (show_find_) render_find_dialog();
    if (show_replace_) render_replace_dialog();
    if (show_goto_) render_goto_dialog();
    if (show_font_) render_font_dialog();
    if (show_spaces_dialog_) render_spaces_dialog();
    if (show_cmd_palette_) render_command_palette();

    render_terminal();
}

// ============================================================================
// Menu Bar
// ============================================================================
void EditorApp::render_menu_bar() {
    if (ImGui::BeginMenuBar()) {
        render_menu_file();
        render_menu_edit();
        render_menu_view();
        ImGui::EndMenuBar();
    }
}

void EditorApp::render_menu_file() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Tab", "Ctrl+N")) new_tab();
        if (ImGui::MenuItem("New Window", "Ctrl+Shift+N")) new_window();
        if (ImGui::MenuItem("Open...", "Ctrl+O")) open_file("");

        // Recent files submenu
        if (!settings_.recent_files.empty()) {
            if (ImGui::BeginMenu("Recent")) {
                for (size_t i = 0; i < settings_.recent_files.size(); i++) {
                    if (ImGui::MenuItem(settings_.recent_files[i].c_str())) {
                        open_file(settings_.recent_files[i]);
                    }
                }
                ImGui::EndMenu();
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Ctrl+S")) save_tab(active_tab_);
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) save_tab_as(active_tab_);
        if (ImGui::MenuItem("Save All", "Ctrl+Alt+S")) save_all();
        ImGui::Separator();
        // Page Setup — placeholder (requires native print dialog)
        if (ImGui::MenuItem("Page Setup...")) { /* TODO: native dialog */ }
        if (ImGui::MenuItem("Print...", "Ctrl+P")) { /* TODO: native print */ }
        ImGui::Separator();
        if (ImGui::MenuItem("Close Tab", "Ctrl+W")) close_tab(active_tab_);
        if (ImGui::MenuItem("Close Window", "Ctrl+Shift+W")) close_window();
        if (ImGui::MenuItem("Exit")) running_ = false;
        ImGui::EndMenu();
    }
}

void EditorApp::render_menu_edit() {
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                get_active_editor()->Undo(); 
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Cut", "Ctrl+X")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                get_active_editor()->Cut(); 
            }
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                get_active_editor()->Copy(); 
            }
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                get_active_editor()->Paste(); 
            }
        }
        if (ImGui::MenuItem("Delete", "Del")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                get_active_editor()->Delete(); 
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Find...", "Ctrl+F")) show_find_ = true;
        if (ImGui::MenuItem("Find Next", "F3")) find_next();
        if (ImGui::MenuItem("Find Previous", "Shift+F3")) find_prev();
        if (ImGui::MenuItem("Replace...", "Ctrl+H")) show_replace_ = true;
        if (ImGui::MenuItem("Go To...", "Ctrl+G")) show_goto_ = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                get_active_editor()->SelectAll(); 
            }
        }
        if (ImGui::MenuItem("Time/Date", "F5")) {
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                auto now = std::chrono::system_clock::now();
                auto t = std::chrono::system_clock::to_time_t(now);
                char buf[64];
                strftime(buf, sizeof(buf), "%H:%M %Y-%m-%d", localtime(&t));
                get_active_editor()->InsertText(buf);
                tabs_[active_tab_].dirty = true;
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Font...")) show_font_ = true;
        ImGui::EndMenu();
    }
}

void EditorApp::render_menu_view() {
    if (ImGui::BeginMenu("View")) {
        if (ImGui::BeginMenu("Zoom")) {
            if (ImGui::MenuItem("Zoom In", "Ctrl++")) zoom_in();
            if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) zoom_out();
            if (ImGui::MenuItem("Restore Default Zoom", "Ctrl+0")) zoom_reset();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        bool sb = settings_.show_status_bar;
        if (ImGui::MenuItem("Status Bar", nullptr, &sb)) toggle_status_bar();
        bool exp = show_file_tree_;
        if (ImGui::MenuItem("Explorer", nullptr, &exp)) toggle_explorer();
        bool ww = settings_.word_wrap;
        if (ImGui::MenuItem("Word Wrap", nullptr, &ww)) toggle_word_wrap();
        bool tabs = settings_.show_tabs;
        if (ImGui::MenuItem("Show Tabs", nullptr, &tabs)) settings_.show_tabs = tabs;
        bool ln = settings_.show_line_numbers;
        if (ImGui::MenuItem("Line Numbers", nullptr, &ln)) toggle_line_numbers();
        bool sp = settings_.show_spaces;
        if (ImGui::MenuItem("Show Spaces", nullptr, &sp)) toggle_spaces();
        bool mp = settings_.show_minimap;
        if (ImGui::MenuItem("Minimap", nullptr, &mp)) toggle_minimap();
        ImGui::Separator();
        if (ImGui::BeginMenu("Bookmarks")) {
            if (ImGui::MenuItem("Toggle Bookmark", "F2")) {
                if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                    auto pos = get_active_editor()->GetCursorPosition();
                    toggle_bookmark(pos.mLine);
                }
            }
            if (ImGui::MenuItem("Next Bookmark", "F4")) next_bookmark();
            if (ImGui::MenuItem("Previous Bookmark", "F3")) prev_bookmark();
            if (ImGui::MenuItem("Clear All Bookmarks")) clear_bookmarks();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Spaces")) {
            if (ImGui::MenuItem("2 Spaces", nullptr, settings_.tab_size == 2)) set_tab_size(2);
            if (ImGui::MenuItem("4 Spaces", nullptr, settings_.tab_size == 4)) set_tab_size(4);
            if (ImGui::MenuItem("8 Spaces", nullptr, settings_.tab_size == 8)) set_tab_size(8);
            if (ImGui::MenuItem("Custom...")) { show_spaces_dialog_ = true; tab_size_temp_ = settings_.tab_size; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Highlight Current Line")) {
            if (ImGui::MenuItem("No highlight", nullptr, settings_.highlight_line == 0)) settings_.highlight_line = 0;
            if (ImGui::MenuItem("Background color", nullptr, settings_.highlight_line == 1)) settings_.highlight_line = 1;
            if (ImGui::MenuItem("Outline frame", nullptr, settings_.highlight_line == 2)) settings_.highlight_line = 2;
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Code Folding")) {
            if (ImGui::MenuItem("Fold All", "Ctrl+Shift+[")) fold_all();
            if (ImGui::MenuItem("Unfold All", "Ctrl+Shift+]")) unfold_all();
            if (ImGui::MenuItem("Toggle Fold", "Ctrl+[")) {
                if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                    auto pos = get_active_editor()->GetCursorPosition();
                    toggle_fold(pos.mLine);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Terminal", "Ctrl+`", &show_terminal_)) {
            if (show_terminal_ && !terminal_process_) start_terminal();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Split")) {
            if (ImGui::MenuItem("Split Horizontal", "Ctrl+Shift+H")) split_horizontal();
            if (ImGui::MenuItem("Split Vertical", "Ctrl+Shift+V")) split_vertical();
            ImGui::Separator();
            if (ImGui::MenuItem("Split and Open File", "Ctrl+Shift+F")) {
                std::string path;
                if (!path.empty()) {
                    open_file_split(path);
                } else {
                    nfdchar_t* out_path = nullptr;
                    nfdresult_t result = NFD_OpenDialog(&out_path, nullptr, 0, nullptr);
                    if (result == NFD_OKAY && out_path) {
                        open_file_split(std::string(out_path));
                        NFD_FreePath(out_path);
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close Split", "Ctrl+Alt+W")) close_split();
            ImGui::Separator();
            if (ImGui::MenuItem("Focus Next", "Ctrl+K")) next_split();
            if (ImGui::MenuItem("Focus Previous", "Ctrl+J")) prev_split();
            ImGui::Separator();
            if (ImGui::MenuItem("Rotate Down", "Ctrl+Alt+K")) rotate_splits();
            if (ImGui::MenuItem("Equal Size", "Ctrl+Alt+E")) equalize_splits();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(settings_.dark_theme ? "Light Theme" : "Dark Theme")) toggle_theme();
ImGui::EndMenu();
    }
}

// ============================================================================
// Editor Area
// ============================================================================
void EditorApp::render_editor_area() {
    // Editor window - dockable
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoSavedSettings);
    
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow)) {
        editor_focused_ = true;
    }

    if (vim_mode_ == VimMode::Command) {
        ImVec4 cmd_color = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetWindowPos(), 
            ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + 3),
            ImColor(cmd_color));
    } else if (editor_focused_) {
        ImVec4 focus_color = ImVec4(0.3f, 0.7f, 0.3f, 1.0f);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetWindowPos(), 
            ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + 3),
            ImColor(focus_color));
    }

    if (tabs_.empty()) {
        new_tab();
    }

    // Tab bar - show if settings allow it
    bool show_tabs = settings_.show_tabs && tabs_.size() > 1;
    static int prev_active_tab = -1;
    
    // Save scroll position when switching away from a tab
    if (prev_active_tab != active_tab_ && prev_active_tab >= 0 && prev_active_tab < (int)tabs_.size()) {
        // Note: TextEditor doesn't have public scroll getters/setters, 
        // but we could track cursor position as proxy
        auto& prev_tab = tabs_[prev_active_tab];
        prev_tab.editor->GetCursorPosition(); // This doesn't return scroll, but we can track cursor
    }
    prev_active_tab = active_tab_;
    
    if (show_tabs) {
        ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs;
        if (ImGui::BeginTabBar("##Tabs", tab_flags)) {
            for (int i = 0; i < (int)tabs_.size(); i++) {
                auto& tab = tabs_[i];
                std::string label = tab.display_name;
                if (tab.dirty) label += " *";

                ImGuiTabItemFlags tab_item_flags = 0;
                if (i == active_tab_) tab_item_flags |= ImGuiTabItemFlags_SetSelected;

                bool open = true;
                if (ImGui::BeginTabItem(label.c_str(), &open, tab_item_flags)) {
                    if (active_tab_ != i) {
                        active_tab_ = i;
                        // Focus on the new tab's editor
                        ImGui::SetWindowFocus("Editor");
                    }
                    
                    // Render gutter with bookmarks and change history
                    render_editor_with_margins();
                    
                    ImGui::EndTabItem();
                }
                if (!open) {
                    close_tab(i);
                    break;
                }
            }
            ImGui::EndTabBar();
        }
    } else {
        // Single tab - just render the editor directly
        if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
            render_editor_with_margins();
        }
    }
    
    // Render status bar at bottom of editor window (docked to editor)
    if (settings_.show_status_bar) {
        float status_height = 24;
        ImVec2 editor_pos = ImGui::GetWindowPos();
        float editor_width = ImGui::GetWindowWidth();
        float editor_height = ImGui::GetWindowHeight();
        
        // Status bar background
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImU32 status_bg = ImColor(0.2f, 0.2f, 0.25f);
        draw_list->AddRectFilled(
            ImVec2(editor_pos.x, editor_pos.y + editor_height - status_height),
            ImVec2(editor_pos.x + editor_width, editor_pos.y + editor_height),
            status_bg);
        
        // Position cursor at bottom
        ImGui::SetCursorPosY(editor_height - status_height);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        
        if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
            auto& tab = tabs_[active_tab_];
            TextEditor* ed = get_active_editor();
            auto pos = ed ? ed->GetCursorPosition() : TextEditor::Coordinates();
            
            ImGui::Text("%s", get_vim_mode_str());
            ImGui::SameLine();
            ImGui::Text(" | ");
            ImGui::SameLine();
            ImGui::Text("Ln %d, Col %d", pos.mLine + 1, pos.mColumn + 1);
            ImGui::SameLine();
            ImGui::Text(" | ");
            ImGui::SameLine();
            ImGui::Text("%s", tab.file_encoding.c_str());
            ImGui::SameLine();
            ImGui::Text(" | ");
            ImGui::SameLine();
            ImGui::Text("%s", tab.line_ending.c_str());
            ImGui::SameLine();
            ImGui::Text(" | ");
            ImGui::SameLine();
            ImGui::Text("%d%%", tabs_[active_tab_].zoom_pct);
            ImGui::SameLine();
            ImGui::Text(" | ");
            ImGui::SameLine();
            ImGui::Text("%s", tab.display_name.c_str());
            ImGui::SameLine();
            ImGui::Text(" | v0.2.28");
        }
        
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    ImGui::End();
}

void EditorApp::render_editor_with_margins() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    TextEditor* editor = get_active_editor();
    if (!editor) return;
    
    // Check if we have splits and render them
    if (!tab.splits.empty()) {
        render_splits(active_tab_);
        return;
    }
    
bool show_margins = settings_.show_bookmark_margin || settings_.show_change_history || settings_.show_line_numbers;
    
    if (show_margins) {
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        
        auto lines = editor->GetTextLines();
        int total_lines = (int)lines.size();
        if (total_lines == 0) total_lines = 1;
        
        // Calculate gutter width based on line number digits
        int max_line = total_lines;
        int digit_count = 1;
        while (max_line >= 10) { max_line /= 10; digit_count++; }
        float gutter_width = 16.0f + (digit_count * 8.0f) + 8.0f; // padding + digits + margin
        
        // Build markers for lookup (using vector search is fine for small sets)
        auto& bookmarks = tab.bookmarks;
        auto& changed_lines = tab.changed_lines;
        auto& folds = tab.folds;
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.18f, 0.21f, 1.0f));
        if (ImGui::BeginChild("##Gutter", ImVec2(gutter_width, -1), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMouseInputs)) {
            if (ImGui::IsMouseClicked(0, false) && !ImGui::IsPopupOpen(0)) {
                // Check for click on gutter
                ImVec2 mouse = ImGui::GetMousePos();
                ImVec2 win_pos = ImGui::GetWindowPos();
                if (mouse.x >= win_pos.x && mouse.x < win_pos.x + gutter_width) {
                    float line_height = ImGui::GetTextLineHeightWithSpacing();
                    int clicked_line = (int)((mouse.y - win_pos.y) / line_height);
                    if (clicked_line >= 0 && clicked_line < total_lines) {
                        // Check if clicked on fold indicator (right side of gutter)
                        float fold_indicator_x = gutter_width - 16;
                        if (mouse.x >= win_pos.x + fold_indicator_x) {
                            // Check for fold at this line
                            bool found_start = false, found_end = false;
                            for (const auto& f : folds) {
                                if (f.first == clicked_line) found_start = true;
                                if (f.second == clicked_line) found_end = true;
                            }
                            if (found_start) {
                                toggle_fold(clicked_line);
                            } else if (found_end) {
                                // Go to fold start to unfold
                                for (const auto& f : folds) {
                                    if (f.second == clicked_line) {
                                        toggle_fold(f.first);
                                        break;
                                    }
                                }
                            }
                        } else {
                            // Toggle bookmark (if clicking on bookmark column)
                            if (settings_.show_bookmark_margin && mouse.x < win_pos.x + 16) {
                                toggle_bookmark(clicked_line);
}
            }
        }
        
        // Render minimap on the right side if enabled
        if (settings_.show_minimap) {
            render_minimap(editor);
        }
    }
}
            
            if (ImGui::BeginPopupContextItem("##GutterPopup")) {
                ImGui::TextDisabled("Gutter");
                ImGui::Separator();
                if (ImGui::MenuItem("Fold All", nullptr, nullptr)) { fold_all(); }
                if (ImGui::MenuItem("Unfold All", nullptr, nullptr)) { unfold_all(); }
                ImGui::Separator();
                ImGui::MenuItem("Line Numbers", nullptr, &settings_.show_line_numbers);
                ImGui::MenuItem("Bookmark", nullptr, &settings_.show_bookmark_margin);
                ImGui::MenuItem("Change History", nullptr, &settings_.show_change_history);
                ImGui::EndPopup();
            }
            
            for (int line = 0; line < total_lines; line++) {
                ImGui::PushID(line);
                
                // Right-align line numbers with padding
                if (settings_.show_line_numbers) {
                    int line_display = line + 1;
                    char line_num[16];
                    sprintf(line_num, "%*d", digit_count, line_display);
                    
                    // Check if current line for highlight
                    bool is_current_line = (line == editor->GetCursorPosition().mLine);
                    if (is_current_line) {
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), line_num);
                    } else {
                        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.5f, 1.0f), line_num);
                    }
                    ImGui::SameLine();
                }
                
                // Bookmark column (far left, 16px)
                if (settings_.show_bookmark_margin) {
                    bool is_bookmarked = std::find(bookmarks.begin(), bookmarks.end(), line) != bookmarks.end();
                    if (is_bookmarked) {
                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), ">");
                    } else {
                        ImGui::TextDisabled(" ");
                    }
                    ImGui::SameLine();
                }
                
                // Change history indicator (shows modified lines in green/red)
                if (settings_.show_change_history) {
                    bool is_changed = std::find(changed_lines.begin(), changed_lines.end(), line) != changed_lines.end();
                    if (is_changed) {
                        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.4f, 1.0f), "|");
                    }
                    ImGui::SameLine();
                }
                
                // Fold indicator (+ or -)
                bool is_fold_start = false, is_fold_end = false;
                for (const auto& f : folds) {
                    if (f.first == line) is_fold_start = true;
                    if (f.second == line) is_fold_end = true;
                }
                if (is_fold_start) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "-");
                } else if (is_fold_end) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "+");
                }
                
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::PopStyleVar();
        
        // Render the text editor
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        editor->Render("TextEditor");
        
        // Render current line highlight
        if (settings_.highlight_line > 0 && editor_focused_) {
            auto cursor = editor->GetCursorPosition();
            auto lines = editor->GetTextLines();
            if (cursor.mLine < (int)lines.size()) {
                // Approximate line height in the editor
                float line_height = ImGui::GetTextLineHeight();
                float y_start = ImGui::GetCursorScreenPos().y - line_height * (lines.size() - cursor.mLine - 1);
                float x_start = ImGui::GetWindowPos().x + ImGui::GetTextLineHeightWithSpacing();
                float line_width = ImGui::GetWindowWidth() - ImGui::GetTextLineHeightWithSpacing();
                
                if (settings_.highlight_line == 1) {
                    // Background color
                    ImVec4 bg_color = settings_.dark_theme ? ImVec4(0.3f, 0.3f, 0.35f, 0.5f) : ImVec4(0.8f, 0.8f, 0.7f, 0.5f);
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(x_start, y_start),
                        ImVec2(x_start + line_width, y_start + line_height),
                        ImColor(bg_color));
                } else if (settings_.highlight_line == 2) {
                    // Outline frame
                    ImVec4 outline_color = ImVec4(0.4f, 0.6f, 0.9f, 1.0f);
                    ImGui::GetWindowDrawList()->AddRect(
                        ImVec2(x_start, y_start),
                        ImVec2(x_start + line_width, y_start + line_height),
                        ImColor(outline_color), 0.f, 0, 2.f);
                }
            }
        }
        
        ImGui::PopStyleVar();
        
        ImGui::EndGroup();
    } else {
        // No gutter needed, just render editor
        editor->Render("TextEditor");
        
        // Render current line highlight
        if (settings_.highlight_line > 0 && editor_focused_) {
            auto cursor = editor->GetCursorPosition();
            auto lines = editor->GetTextLines();
            if (cursor.mLine < (int)lines.size()) {
                float line_height = ImGui::GetTextLineHeight();
                float y_start = ImGui::GetCursorScreenPos().y - line_height * (lines.size() - cursor.mLine - 1);
                float x_start = ImGui::GetWindowPos().x + ImGui::GetTextLineHeightWithSpacing();
                float line_width = ImGui::GetWindowWidth() - ImGui::GetTextLineHeightWithSpacing();
                
                if (settings_.highlight_line == 1) {
                    ImVec4 bg_color = settings_.dark_theme ? ImVec4(0.3f, 0.3f, 0.35f, 0.5f) : ImVec4(0.8f, 0.8f, 0.7f, 0.5f);
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(x_start, y_start),
                        ImVec2(x_start + line_width, y_start + line_height),
                        ImColor(bg_color));
                } else if (settings_.highlight_line == 2) {
                    ImVec4 outline_color = ImVec4(0.4f, 0.6f, 0.9f, 1.0f);
                    ImGui::GetWindowDrawList()->AddRect(
                        ImVec2(x_start, y_start),
                        ImVec2(x_start + line_width, y_start + line_height),
                        ImColor(outline_color), 0.f, 0, 2.f);
                }
            }
        }
    }
}

// ============================================================================
// Sidebar - file explorer, git changes, and symbol outline
// ============================================================================
void EditorApp::render_sidebar() {
    if (!show_file_tree_) return;
    
    ImGui::Begin("Explorer", nullptr, ImGuiWindowFlags_NoTitleBar);
    
    if (ImGui::BeginTabBar("##SidebarTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Files", nullptr, ImGuiTabItemFlags_None)) {
            render_file_tree();
            ImGui::EndTabItem();
        }
        if (show_git_changes_ && ImGui::BeginTabItem("Git", nullptr, ImGuiTabItemFlags_None)) {
            render_git_changes();
            ImGui::EndTabItem();
        }
        if (show_symbol_outline_ && ImGui::BeginTabItem("Symbols", nullptr, ImGuiTabItemFlags_None)) {
            render_symbol_outline();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void EditorApp::render_file_tree() {
    ImGui::Text("Files");
    ImGui::Separator();
    
    static std::unordered_map<std::string, bool> expanded_dirs;
    static std::string current_dir = ".";
    
    std::string dir_path = settings_.last_open_dir.empty() ? "." : settings_.last_open_dir;
    
    // Allow navigation to parent directory
    std::filesystem::path parent = std::filesystem::path(dir_path).parent_path();
    if (!parent.empty() && parent.string() != dir_path) {
        if (ImGui::Selectable("[..]", false, ImGuiSelectableFlags_DontClosePopups)) {
            settings_.last_open_dir = parent.string();
            current_dir = parent.string();
        }
    }
    
    render_dir_tree(dir_path, expanded_dirs);
}

void EditorApp::render_dir_tree(const std::string& dir_path, std::unordered_map<std::string, bool>& expanded_dirs) {
    if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path)) return;
    
    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        std::string name = entry.path().filename().string();
        std::string full_path = entry.path().string();
        
        if (!name.empty() && name[0] == '.') continue;
        
        if (entry.is_directory()) {
            ImGui::PushID(full_path.c_str());
            bool is_expanded = expanded_dirs[full_path];
            
            std::string arrow = is_expanded ? "[-] " : "[+] ";
            if (ImGui::Selectable(arrow.c_str(), false)) {
                expanded_dirs[full_path] = !is_expanded;
            }
            ImGui::SameLine();
            
            ImGui::Text("%s/", name.c_str());
            
            if (is_expanded) {
                ImGui::Indent(16);
                render_dir_tree(full_path, expanded_dirs);
                ImGui::Unindent(16);
            }
            
            ImGui::PopID();
        } else {
            if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_DontClosePopups)) {
                bool already_open = false;
                for (size_t i = 0; i < tabs_.size(); i++) {
                    if (tabs_[i].file_path == full_path) {
                        active_tab_ = i;
                        ImGui::SetWindowFocus("Editor");
                        already_open = true;
                        break;
                    }
                }
                if (!already_open) {
                    open_file(full_path);
                }
            }
        }
    }
}

void EditorApp::render_git_changes() {
    if (!show_git_changes_) return;
    
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.0f, 1.0f), "Git: %s", git_branch_.c_str());
    ImGui::Separator();
    
    if (!std::filesystem::exists(".git")) {
        ImGui::TextDisabled("(Not a git repository)");
        return;
    }
    
    std::vector<std::string> modified_files;
    
    // Try to get modified files from git (cross-platform)
#ifdef _WIN32
    FILE* pipe = _popen("git diff --name-only --porcelain 2>nul", "r");
#else
    FILE* pipe = popen("git diff --name-only --porcelain 2>/dev/null", "r");
#endif
    if (pipe) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) {
            std::string line = buf;
            if (!line.empty() && line[0] == ' ' || line[0] == '?') {
                std::string path = line.substr(2);
                path.erase(path.find('\n'), std::string::npos);
                if (!path.empty()) modified_files.push_back(path);
            }
        }
        _pclose(pipe);
    }
    
    // Fallback: scan all files in current dir if no git
    if (modified_files.empty()) {
        std::string dir_path = settings_.last_open_dir.empty() ? "." : settings_.last_open_dir;
        for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                std::string name = entry.path().filename().string();
                if (!name.empty() && name[0] != '.') {
                    modified_files.push_back(entry.path().string());
                }
            }
        }
    }
    
    if (modified_files.empty()) {
        ImGui::TextDisabled("(No modified files)");
        return;
    }
    
    ImGui::Text("Changes (%zu):", modified_files.size());
    for (const auto& path : modified_files) {
        std::string name = std::filesystem::path(path).filename().string();
        if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_DontClosePopups)) {
            // Switch to or open the file
            bool already_open = false;
            for (size_t i = 0; i < tabs_.size(); i++) {
                if (tabs_[i].file_path == path) {
                    active_tab_ = i;
                    ImGui::SetWindowFocus("Editor");
                    already_open = true;
                    break;
                }
            }
            if (!already_open) {
                open_file(path);
            }
        }
    }
}

void EditorApp::render_breadcrumb() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    
    std::string path = tab.file_path.empty() ? "untitled" : tab.file_path;
    ImGui::Text("%s", path.c_str());
}

void EditorApp::render_symbol_outline() {
    if (!show_symbol_outline_) return;
    
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) {
        ImGui::TextDisabled("Symbols (no file)");
        return;
    }
    
    TextEditor* ed = get_active_editor();
    if (!ed) return;
    
    std::string text = ed->GetText();
    std::istringstream iss(text);
    std::string line;
    int line_num = 0;
    int symbol_count = 0;
    
    while (std::getline(iss, line)) {
        line_num++;
        
        // Skip comments and empty lines
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        if (trimmed.empty() || trimmed.substr(0, 2) == "//" || trimmed.substr(0, 2) == "/*") continue;
        
        // Look for different symbol types with better patterns
        std::string icon;
        ImVec4 color(0.6f, 0.6f, 0.6f, 1.0f);
        
        if (trimmed.find("class ") == 0) {
            icon = "[C] ";
            color = ImVec4(0.4f, 0.6f, 1.0f, 1.0f);  // blue
        } else if (trimmed.find("struct ") == 0) {
            icon = "[S] ";
            color = ImVec4(0.4f, 0.7f, 0.5f, 1.0f);  // green
        } else if (trimmed.find("enum ") == 0) {
            icon = "[E] ";
            color = ImVec4(0.7f, 0.5f, 0.3f, 1.0f);  // brown
        } else if (trimmed.find("void ") != std::string::npos || 
                  trimmed.find("int ") != std::string::npos ||
                  trimmed.find("bool ") != std::string::npos ||
                  trimmed.find("std::") != std::string::npos ||
                  trimmed.find("auto ") != std::string::npos) {
            // Try to detect function (has paren after type)
            size_t paren = trimmed.find('(');
            size_t name_start = trimmed.find_first_of(" \t*&");
            if (paren != std::string::npos && name_start != std::string::npos && name_start < paren) {
                icon = "[F] ";
                color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);  // yellow
            } else {
                continue;  // Skip variable declarations
            }
        } else if (trimmed.find("def ") == 0 || trimmed.find("fn ") == 0 || trimmed.find("func ") == 0) {
            icon = "[F] ";
            color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
        } else if (trimmed.find("#define ") == 0) {
            icon = "[D] ";
            color = ImVec4(0.8f, 0.4f, 0.8f, 1.0f);  // purple
        } else if (trimmed.find("const ") != std::string::npos && trimmed.find('=') != std::string::npos) {
            icon = "[V] ";
            color = ImVec4(0.5f, 0.8f, 0.8f, 1.0f);  // cyan
        } else {
            continue;
        }
        
        // Trim and clean the symbol display
        std::string symbol = line;
        symbol.erase(0, symbol.find_first_not_of(" \t"));
        
        // Remove trailing comments and braces
        size_t comment_pos = symbol.find("//");
        if (comment_pos != std::string::npos) symbol = symbol.substr(0, comment_pos);
        size_t brace_pos = symbol.find('{');
        if (brace_pos != std::string::npos) symbol = symbol.substr(0, brace_pos);
        
        // Trim end
        while (!symbol.empty() && (symbol.back() == ' ' || symbol.back() == '\t' || symbol.back() == ';')) {
            symbol.pop_back();
        }
        
        if (symbol.length() > 35) symbol = symbol.substr(0, 35) + "...";
        
        ImGui::PushID(line_num);
        if (ImGui::Selectable((icon + symbol).c_str(), false, ImGuiSelectableFlags_DontClosePopups)) {
            // Jump to this line
            ed->SetCursorPosition(TextEditor::Coordinates(line_num - 1, 0));
            ed->SetSelection(TextEditor::Coordinates(line_num - 1, 0), TextEditor::Coordinates(line_num - 1, 0));
            ImGui::SetWindowFocus("Editor");
        }
        ImGui::PopID();
        symbol_count++;
    }
    
    if (symbol_count == 0) {
        ImGui::TextDisabled("(No symbols found)");
    }
}

// ============================================================================
// Status Bar
// ============================================================================
void EditorApp::render_status_bar() {
    if (!settings_.show_status_bar) return;
    
    float status_height = 22;
    float cmd_height = vim_mode_ == VimMode::Command ? 22 : 0;
    
    // Get editor window dimensions
    ImVec2 editor_pos = ImGui::GetWindowPos();
    float editor_width = ImGui::GetWindowWidth();
    float editor_height = ImGui::GetWindowHeight();
    
    // Command bar - inside editor, one line high, positioned at very bottom of editor
    if (vim_mode_ == VimMode::Command) {
        float cmd_y = editor_height - cmd_height;
        ImGui::SetCursorPosY(cmd_y);
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImU32 cmd_bg = ImColor(0.1f, 0.1f, 0.2f);
        draw_list->AddRectFilled(
            ImVec2(editor_pos.x, editor_pos.y + cmd_y),
            ImVec2(editor_pos.x + editor_width, editor_pos.y + editor_height),
            cmd_bg);
        
        ImGui::SetCursorPosY(cmd_y);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
        
        ImGui::Text(":");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(editor_width - 30);
        if (ImGui::InputText("##cmd", vim_command_input_, sizeof(vim_command_input_), ImGuiInputTextFlags_EnterReturnsTrue)) {
            execute_vim_command(vim_command_input_);
            vim_command_input_[0] = '\0';
            vim_mode_ = VimMode::Normal;
        }
        
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        
        // Move status bar above command bar
        editor_height -= cmd_height;
    }
    
    // Status bar at bottom of editor
    ImGui::SetCursorPosY(editor_height - status_height);
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 status_bg = ImColor(0.2f, 0.2f, 0.25f);
    draw_list->AddRectFilled(
        ImVec2(editor_pos.x, editor_pos.y + editor_height - status_height),
        ImVec2(editor_pos.x + editor_width, editor_pos.y + editor_height),
        status_bg);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    
    if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
        auto& tab = tabs_[active_tab_];
        TextEditor* editor = get_active_editor();
        auto pos = editor ? editor->GetCursorPosition() : TextEditor::Coordinates();

        const char* mode_str = get_vim_mode_str();
        const char* sep = "|";
        
        // Get directory info
        std::string dir_info = ".";
        if (!tab.file_path.empty()) {
            std::filesystem::path p(tab.file_path);
            if (p.has_parent_path()) {
                std::string dir = p.parent_path().string();
                if (std::filesystem::exists(dir + "/.git")) {
                    dir_info = git_branch_;
                } else {
                    dir_info = p.parent_path().filename().string();
                }
            }
        }
        
        // Exact format: | MODE | filename | * | Ln,Col | Git:branch | encoding | CRLF | Tab:n | v0.2.28 |
        ImGui::Text("%s", mode_str);
        ImGui::SameLine();
        ImGui::Text("%s", sep);
        ImGui::SameLine();
        
        std::string display = tab.display_name + (tab.dirty ? " *" : "");
        ImGui::Text("%s", display.c_str());
        ImGui::SameLine();
        ImGui::Text("%s", sep);
        ImGui::SameLine();
        
        ImGui::Text("Ln %d, Col %d", pos.mLine + 1, pos.mColumn + 1);
        ImGui::SameLine();
        ImGui::Text("%s", sep);
        ImGui::SameLine();
        
        ImGui::Text("Git: %s", dir_info.c_str());
        ImGui::SameLine();
        ImGui::Text("%s", sep);
        ImGui::SameLine();
        
        ImGui::Text("%s", tab.file_encoding.c_str());
        ImGui::SameLine();
        ImGui::Text("%s", sep);
        ImGui::SameLine();
        
        // Line ending - clickable
        if (ImGui::Selectable(tab.line_ending.c_str(), false)) {
            ImGui::OpenPopup("LineEndingPopup");
        }
        if (ImGui::BeginPopup("LineEndingPopup")) {
            if (ImGui::MenuItem("LF", nullptr, tab.line_ending == "LF")) { tab.line_ending = "LF"; }
            if (ImGui::MenuItem("CRLF", nullptr, tab.line_ending == "CRLF")) { tab.line_ending = "CRLF"; }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::Text("%s", sep);
        ImGui::SameLine();
        
        // Tab size - clickable
        std::string tab_id = "Tab: " + std::to_string(settings_.tab_size);
        if (ImGui::Selectable(tab_id.c_str(), false)) {
            ImGui::OpenPopup("TabSizePopup");
        }
        if (ImGui::BeginPopup("TabSizePopup")) {
            for (int i = 1; i <= 8; i++) {
                if (ImGui::MenuItem(("Tab: " + std::to_string(i)).c_str(), nullptr, settings_.tab_size == i)) {
                    settings_.tab_size = i;
                    for (auto& t : tabs_) t.editor->SetTabSize(i);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::Text("%s", sep);
        ImGui::SameLine();
        
        // Version with git hash
        ImGui::Text("v0.2.32");
    }
    
ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void EditorApp::render_minimap(TextEditor* editor) {
    if (!editor) return;
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 vp = viewport->Pos;
    ImVec2 vs = viewport->Size;
    
    float sidebar_width = show_file_tree_ ? 220.0f : 0.0f;
    float editor_x = vp.x + sidebar_width + 60;
    float editor_width = vs.x - sidebar_width - 60 - 90;
    float editor_height = vs.y - 80;
    float minimap_width = 80;
    float minimap_x = editor_x + editor_width + 5;
    
    auto lines = editor->GetTextLines();
    int total_lines = (int)lines.size();
    if (total_lines == 0) total_lines = 1;
    float scale = (editor_height - 20) / (float)total_lines;
    if (scale > 4) scale = 4;
    if (scale < 0.5f) scale = 0.5f;
    
    ImGui::SetNextWindowPos(ImVec2(minimap_x, vp.y + 8));
    ImGui::SetNextWindowSize(ImVec2(minimap_width, editor_height - 16));
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                            ImGuiWindowFlags_NoInputs;
    
    if (ImGui::Begin("##Minimap", nullptr, flags)) {
        auto cursor = editor->GetCursorPosition();
        
        for (int i = 0; i < total_lines; i++) {
            float y = vp.y + 10 + (i * scale);
            if (y < vp.y + 10 || y > vp.y + editor_height - 10) continue;
            
            if (i == cursor.mLine) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(minimap_x, y),
                    ImVec2(minimap_x + minimap_width - 4, y + scale - 1),
                    IM_COL32(120, 120, 150, 255));
            } else {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(minimap_x, y),
                    ImVec2(minimap_x + minimap_width - 4, y + scale - 1),
                    IM_COL32(70, 70, 90, 200));
            }
        }
    }
    ImGui::End();
    
    if (ImGui::Begin("##MinimapInput", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::IsMouseClicked(0)) {
            ImVec2 mouse = ImGui::GetMousePos();
            if (mouse.x >= minimap_x && mouse.x < minimap_x + minimap_width) {
                float rel_y = mouse.y - (vp.y + 10);
                int target_line = (int)(rel_y / scale);
                if (target_line < 0) target_line = 0;
                if (target_line >= total_lines) target_line = total_lines - 1;
                editor->SetCursorPosition(TextEditor::Coordinates(target_line, 0));
                editor->SetSelection(TextEditor::Coordinates(target_line, 0), TextEditor::Coordinates(target_line, 0));
            }
        }
    }
    ImGui::End();
}

void EditorApp::render_floating_command() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float cmd_height = 32;
    float y = viewport->Pos.y + viewport->Size.y - cmd_height;
    
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, y));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, cmd_height));
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    
    if (ImGui::Begin("##CommandLine", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), ":");
        ImGui::SameLine();
        
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::SetKeyboardFocusHere();
        
        // Handle up/down arrow for command history
        if (ImGui::IsItemActive()) {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                if (!command_history_.empty() && history_index_ < (int)command_history_.size() - 1) {
                    history_index_++;
                    strncpy(vim_cmd_input_, command_history_[history_index_].c_str(), sizeof(vim_cmd_input_) - 1);
                }
            } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
if (history_index_ > 0) {
                    history_index_--;
                    strncpy(vim_cmd_input_, command_history_[history_index_].c_str(), sizeof(vim_cmd_input_) - 1);
                    vim_cmd_input_[sizeof(vim_cmd_input_) - 1] = '\0';
                    } else if (history_index_ == 0) {
                    history_index_ = -1;
                    vim_cmd_input_[0] = '\0';
                }
            }
            
            // Handle Escape to cancel command mode
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                vim_mode_ = VimMode::Normal;
                vim_command_buffer_.clear();
                vim_cmd_input_[0] = '\0';
                history_index_ = -1;
                ImGui::SetWindowFocus("Editor");
                return;
            }
            
            // Handle Tab for file autocomplete
            if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
                std::string current = vim_cmd_input_;
                // Only autocomplete for :e command
                if (current.substr(0, 2) == ":e " || current.substr(0, 1) == "e") {
                    std::string prefix = current;
                    if (current == ":e" || current == "e") prefix = ":e ";
                    else if (current.substr(0, 2) == ":e") prefix = current + " ";
                    else if (current.substr(0, 1) == "e") prefix = ":e " + current.substr(1);
                    
                    // Get files from explorer tree directory
                    std::string dir = settings_.last_open_dir.empty() ? "." : settings_.last_open_dir;
                    std::vector<std::string> matches;
                    if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
                        std::string search = "";
                        if (prefix.substr(2, 1) == " ") search = prefix.substr(3);
                        
                        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                            std::string name = entry.path().filename().string();
                            if (search.empty() || name.find(search) == 0) {
                                if (!entry.is_directory()) matches.push_back(name);
                            }
                        }
                    }
                    if (!matches.empty()) {
                        std::sort(matches.begin(), matches.end());
                        strncpy(vim_cmd_input_, (":e " + matches[0]).c_str(), sizeof(vim_cmd_input_) - 1);
                    }
                }
            }
        }
        
        if (ImGui::InputText("##cmd", vim_cmd_input_, sizeof(vim_cmd_input_), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (vim_cmd_input_[0] != '\0') {
                vim_command_buffer_ = vim_cmd_input_;
                execute_vim_command(vim_command_buffer_);
                command_history_.push_back(vim_cmd_input_);
                history_index_ = -1;
            }
            vim_command_buffer_.clear();
            vim_cmd_input_[0] = '\0';
            vim_mode_ = VimMode::Normal;
            ImGui::SetWindowFocus("Editor");
        }
        
        ImGui::SetItemDefaultFocus();
    }
    
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void EditorApp::render_command_line() {
    // Command line is now rendered inside render_status_bar
}

#ifdef _WIN32
void EditorApp::start_terminal() {
    if (terminal_process_) return;
    
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hStdInRd = NULL, hStdInWr = NULL;
    HANDLE hStdOutRd = NULL, hStdOutWr = NULL;
    
    CreatePipe(&hStdInRd, &hStdInWr, &sa, 0);
    CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0);
    
    SetHandleInformation(hStdInWr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOA si = {sizeof(STARTUPINFOA)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdInRd;
    si.hStdOutput = hStdOutWr;
    si.hStdError = hStdOutWr;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    const char* shell = getenv("COMSPEC") ? getenv("COMSPEC") : "cmd.exe";
    
    if (CreateProcessA(nullptr, (LPSTR)shell, nullptr, nullptr, TRUE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        terminal_process_ = (void*)pi.hProcess;
        terminal_stdin_ = (void*)hStdInWr;
        terminal_stdout_ = (void*)hStdOutRd;
        CloseHandle(pi.hThread);
        CloseHandle(hStdInRd);
        CloseHandle(hStdOutWr);
        terminal_output_ = "Terminal started: ";
        terminal_output_ += shell;
        terminal_output_ += "\r\n";
    } else {
        CloseHandle(hStdInRd);
        CloseHandle(hStdInWr);
        CloseHandle(hStdOutRd);
        CloseHandle(hStdOutWr);
    }
}
#else
#include <pty.h>
#include <utmp.h>
#include <unistd.h>
#include <termios.h>

void EditorApp::start_terminal() {
    if (terminal_process_) return;
    
    int master_fd;
    pid_t pid = forkpty(&master_fd, nullptr, nullptr, nullptr);
    
    if (pid < 0) {
        terminal_output_ = "Failed to fork terminal\r\n";
        return;
    }
    
    if (pid == 0) {
        // Child process - exec shell
        const char* shell = getenv("SHELL") ? getenv("SHELL") : "/bin/sh";
        execl(shell, shell, nullptr);
        perror("execl");
        exit(1);
    } else {
        // Parent process
        terminal_process_ = (void*)(intptr_t)pid;
        terminal_stdin_ = (void*)(intptr_t)master_fd;
        terminal_stdout_ = (void*)(intptr_t)master_fd;
        terminal_output_ = "Terminal started. Type commands.\r\n";
    }
}
#endif

void EditorApp::update_terminal_output() {
    if (!terminal_stdout_ || !terminal_process_) return;
    
#ifdef _WIN32
    DWORD avail = 0;
    if (PeekNamedPipe((HANDLE)terminal_stdout_, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
        char buf[1024] = {};
        DWORD read = 0;
        if (ReadFile((HANDLE)terminal_stdout_, buf, sizeof(buf) - 1, &read, nullptr)) {
            if (read > 0) {
                terminal_output_ += std::string(buf, read);
                if (terminal_output_.size() > 10000) {
                    terminal_output_ = terminal_output_.substr(terminal_output_.size() - 5000);
                }
            }
        }
    }
    
    DWORD exitCode = 0;
    if (GetExitCodeProcess((HANDLE)terminal_process_, &exitCode)) {
        if (exitCode != STILL_ACTIVE) {
            terminal_output_ += "\r\n[Process exited]\r\n";
            if (terminal_stdin_) CloseHandle((HANDLE)terminal_stdin_);
            if (terminal_stdout_) CloseHandle((HANDLE)terminal_stdout_);
            terminal_process_ = nullptr;
            terminal_stdin_ = nullptr;
            terminal_stdout_ = nullptr;
        }
    }
#else
    int fd = (int)(intptr_t)terminal_stdout_;
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    
    struct timeval tv = {0, 0};
    if (select(fd + 1, &read_fds, nullptr, nullptr, &tv) > 0) {
        char buf[1024] = {};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            terminal_output_ += std::string(buf, n);
            if (terminal_output_.size() > 10000) {
                terminal_output_ = terminal_output_.substr(terminal_output_.size() - 5000);
            }
        }
    }
    
    int status;
    if (waitpid((pid_t)(intptr_t)terminal_process_, &status, WNOHANG) > 0) {
        terminal_output_ += "\r\n[Process exited]\r\n";
        if (terminal_stdin_) close((int)(intptr_t)terminal_stdin_);
        if (terminal_stdout_) close((int)(intptr_t)terminal_stdout_);
        terminal_process_ = nullptr;
        terminal_stdin_ = nullptr;
        terminal_stdout_ = nullptr;
    }
#endif
}

void EditorApp::render_terminal() {
    if (!show_terminal_) return;
    
    update_terminal_output();
    
    ImGui::SetNextWindowSize(ImVec2(600, 300));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    if (ImGui::Begin("Terminal", nullptr, flags)) {
        float term_scale = terminal_zoom_pct_ / 100.0f;
        float old_scale = ImGui::GetIO().FontGlobalScale;
        ImGui::GetIO().FontGlobalScale = old_scale * term_scale;
        
        // Show terminal output - scrollable
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        
        ImGui::BeginChild("term_output", ImVec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight() - 30), true);
        ImGui::Text("%s", terminal_output_.c_str());
        ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        
        ImGui::GetIO().FontGlobalScale = old_scale;
        
        // Simple input at bottom
        ImGui::Separator();
        static char input_buf[256] = "";
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 40);
        
        if (ImGui::InputText("##term", input_buf, sizeof(input_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (terminal_stdin_ && strlen(input_buf) > 0) {
                std::string cmd = input_buf;
                cmd += "\n";
#ifdef _WIN32
                DWORD written;
                WriteFile((HANDLE)terminal_stdin_, cmd.c_str(), (DWORD)cmd.size(), &written, nullptr);
#else
                write((int)(intptr_t)terminal_stdin_, cmd.c_str(), cmd.size());
#endif
                terminal_history_.push_back(input_buf);
                input_buf[0] = '\0';
            }
        }
        
ImGui::SameLine();
        if (ImGui::Button("x")) {
            show_terminal_ = false;
        }
        
        // Direct keyboard input when terminal is focused
        static std::string current_line;
        
        if (ImGui::IsWindowFocused()) {
            ImGuiIO& io = ImGui::GetIO();
            
            // Terminal zoom (Ctrl++/-)
            if (io.KeyCtrl && !io.KeyAlt) {
                if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) { terminal_zoom_in(); return; }
                if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) { terminal_zoom_out(); return; }
                if (ImGui::IsKeyPressed(ImGuiKey_0)) { terminal_zoom_reset(); return; }
            }
            
            // Handle Enter - send command
            if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (terminal_stdin_ && !current_line.empty()) {
                    std::string cmd = current_line + "\n";
#ifdef _WIN32
                    DWORD written;
                    WriteFile((HANDLE)terminal_stdin_, cmd.c_str(), (DWORD)cmd.size(), &written, nullptr);
#else
                    write((int)(intptr_t)terminal_stdin_, cmd.c_str(), cmd.size());
#endif
                    terminal_history_.push_back(current_line);
                }
                current_line.clear();
            }
            // Handle Backspace
            else if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !current_line.empty()) {
                current_line.pop_back();
                char bs = 8;
                if (terminal_stdin_) {
#ifdef _WIN32
                    DWORD written;
                    WriteFile((HANDLE)terminal_stdin_, &bs, 1, &written, nullptr);
#else
                    write((int)(intptr_t)terminal_stdin_, &bs, 1);
#endif
                }
            }
            // Handle Escape - clear
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                current_line.clear();
            }
            // Handle printable ASCII keys
            else {
                for (int i = 32; i <= 126; i++) {
                    if (ImGui::IsKeyPressed((ImGuiKey)i)) {
                        char c = (char)i;
                        current_line += c;
                        if (terminal_stdin_) {
#ifdef _WIN32
                            DWORD written;
                            WriteFile((HANDLE)terminal_stdin_, &c, 1, &written, nullptr);
#else
                            write((int)(intptr_t)terminal_stdin_, &c, 1);
#endif
                        }
                    }
                }
            }
        }
        
        // Show prompt > and current line
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "> ");
        ImGui::SameLine();
        ImGui::Text("%s_", current_line.c_str());  // _ is cursor
        
ImGui::PopStyleVar();
        ImGui::End();
    }
    ImGui::PopStyleVar();
}

// ============================================================================
// Dialogs
// ============================================================================
void EditorApp::render_find_dialog() {
    ImGui::OpenPopup("Find");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Find", &show_find_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("Find:", find_text_, sizeof(find_text_), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Checkbox("Match case", &find_match_case_);
        ImGui::Checkbox("Whole word", &find_whole_word_);
        ImGui::Checkbox("Wrap around", &find_wrap_);

        if (ImGui::Button("Find Next") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            find_next();
        }
        ImGui::SameLine();
        if (ImGui::Button("Find Previous")) {
            find_prev();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            show_find_ = false;
        }

        ImGui::EndPopup();
    }
}

void EditorApp::render_replace_dialog() {
    ImGui::OpenPopup("Replace");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Replace", &show_replace_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("Find:", find_text_, sizeof(find_text_));
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("Replace:", replace_text_, sizeof(replace_text_));

        ImGui::Checkbox("Match case", &find_match_case_);

        if (ImGui::Button("Replace")) { replace_one(); }
        ImGui::SameLine();
        if (ImGui::Button("Replace All")) { replace_all(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { show_replace_ = false; }

        ImGui::EndPopup();
    }
}

void EditorApp::render_goto_dialog() {
    ImGui::OpenPopup("Go To Line");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Go To Line", &show_goto_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Line number:");
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##gotoline", goto_line_buf_, sizeof(goto_line_buf_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            int line = atoi(goto_line_buf_);
            if (line > 0 && active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                get_active_editor()->SetCursorPosition(
                    TextEditor::Coordinates(line - 1, 0));
            }
            show_goto_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Go")) {
            int line = atoi(goto_line_buf_);
            if (line > 0 && active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                get_active_editor()->SetCursorPosition(
                    TextEditor::Coordinates(line - 1, 0));
            }
            show_goto_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) show_goto_ = false;

        ImGui::EndPopup();
    }
}

void EditorApp::render_font_dialog() {
    static std::vector<std::string> font_list;
    static bool fonts_loaded = false;
    
    if (!fonts_loaded) {
        font_list.clear();
#if defined(_WIN32)
        std::vector<std::string> extensions = {".ttf", ".otf", ".ttc"};
        
        for (const auto& ext : extensions) {
            std::string search_path = std::string("C:\\Windows\\Fonts\\*") + ext;
            std::wstring wpath;
            wpath.assign(search_path.begin(), search_path.end());
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(wpath.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    std::wstring ws(fd.cFileName);
                    std::string name(ws.begin(), ws.end());
                    size_t dot = name.rfind('.');
                    if (dot != std::string::npos) name = name.substr(0, dot);
                    
                    if (!name.empty()) {
                        bool found = false;
                        for (const auto& existing : font_list) {
                            if (existing == name) { found = true; break; }
                        }
                        if (!found) font_list.push_back(name);
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        }
        
        // Algorithm: clean suffix patterns (MT, NB, BK, LI, etc.)
        for (auto& font : font_list) {
            std::string cleaned;
            for (size_t i = 0; i < font.length(); ++i) {
                char c = font[i];
                // Skip common suffixes
                if (i + 1 < font.length() && 
                    ((font[i] == 'M' && font[i+1] == 'T') ||
                     (font[i] == 'N' && font[i+1] == 'B') ||
                     (font[i] == 'B' && font[i+1] == 'K') ||
                     (font[i] == 'L' && font[i+1] == 'I'))) {
                    break;
                }
                if (i == 0 && c >= '0' && c <= '9') continue;
                cleaned += c;
            }
            if (!cleaned.empty()) font = cleaned;
        }
        
        std::sort(font_list.begin(), font_list.end());
        font_list.erase(std::unique(font_list.begin(), font_list.end()), font_list.end());
#endif
        fonts_loaded = true;
    }
    
    ImGui::OpenPopup("Font Settings");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // Initialize temp values from current settings when opening
    if (show_font_ && font_name_temp_.empty()) {
        font_name_temp_ = settings_.font_name;
    }

    if (ImGui::BeginPopupModal("Font Settings", &show_font_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Font Family:");
        ImGui::SetNextItemWidth(250);
        if (ImGui::BeginCombo("##fontfamily", font_name_temp_.empty() ? "Default" : font_name_temp_.c_str())) {
            for (const auto& font : font_list) {
                if (ImGui::Selectable(font.c_str())) {
                    font_name_temp_ = font;
                }
            }
            ImGui::EndCombo();
        }
        
        ImGui::Text("Font Size (8-48):");
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("##fontsize", &font_size_temp_);
        font_size_temp_ = (std::max)(8, (std::min)(48, font_size_temp_));
        
        ImGui::Separator();
        ImGui::TextUnformatted("Note: Font size changes apply to all tabs.");
        ImGui::TextUnformatted("Font family changes require application restart.");

        if (ImGui::Button("Apply")) {
            settings_.font_size = font_size_temp_;
            settings_.font_name = font_name_temp_;
            rebuild_fonts();
        }
        ImGui::SameLine();
        if (ImGui::Button("OK")) {
            settings_.font_size = font_size_temp_;
            settings_.font_name = font_name_temp_;
            rebuild_fonts();
            show_font_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) show_font_ = false;

        ImGui::EndPopup();
    }
}

void EditorApp::render_command_palette() {
    ImGui::OpenPopup("Command Palette");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 300));

    if (ImGui::BeginPopupModal("Command Palette", &show_cmd_palette_, ImGuiWindowFlags_NoResize)) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##cmd", cmd_buf_, sizeof(cmd_buf_), ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            show_cmd_palette_ = false;
        }

        // Simple command matching
        if (strlen(cmd_buf_) > 0) {
            ImGui::Separator();
            if (ImGui::Selectable("File: Open")) {
                open_file("");
                show_cmd_palette_ = false;
                cmd_buf_[0] = '\0';
            }
            if (ImGui::Selectable("File: Save")) {
                save_tab(active_tab_);
                show_cmd_palette_ = false;
                cmd_buf_[0] = '\0';
            }
            if (ImGui::Selectable("File: Save All")) {
                save_all();
                show_cmd_palette_ = false;
                cmd_buf_[0] = '\0';
            }
            if (ImGui::Selectable("Edit: Find")) {
                show_find_ = true;
                show_cmd_palette_ = false;
                cmd_buf_[0] = '\0';
            }
            if (ImGui::Selectable("Edit: Replace")) {
                show_replace_ = true;
                show_cmd_palette_ = false;
                cmd_buf_[0] = '\0';
            }
            if (ImGui::Selectable("View: Toggle Theme")) {
                toggle_theme();
                show_cmd_palette_ = false;
                cmd_buf_[0] = '\0';
            }
            if (ImGui::Selectable("View: Toggle Word Wrap")) {
                toggle_word_wrap();
                show_cmd_palette_ = false;
                cmd_buf_[0] = '\0';
            }
            if (ImGui::Selectable("View: Toggle Status Bar")) {
                toggle_status_bar();
                show_cmd_palette_ = false;
                cmd_buf_[0] = '\0';
            }
            if (ImGui::Selectable("View: Toggle Line Numbers")) {
                toggle_line_numbers();
                show_cmd_palette_ = false;
                cmd_buf_[0] = '\0';
            }
        }

        ImGui::EndPopup();
    }
}

void EditorApp::render_spaces_dialog() {
    ImGui::OpenPopup("Tab Size");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Tab Size", &show_spaces_dialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Tab size (1-16):");
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("##tabsize", &tab_size_temp_);
        tab_size_temp_ = (std::max)(1, (std::min)(16, tab_size_temp_));

        if (ImGui::Button("Apply")) {
            set_tab_size(tab_size_temp_);
        }
        ImGui::SameLine();
        if (ImGui::Button("OK")) {
            set_tab_size(tab_size_temp_);
            show_spaces_dialog_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) show_spaces_dialog_ = false;

        ImGui::EndPopup();
    }
}

// ============================================================================
// Splits
// ============================================================================
TextEditor* EditorApp::get_active_editor() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return nullptr;
    auto& tab = tabs_[active_tab_];
    if (tab.splits.empty()) return tab.editor;
    return tab.splits[tab.active_split]->editor;
}

void EditorApp::split_horizontal() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    TextEditor* active_editor = get_active_editor();
    if (!active_editor) return;
    
    TextEditor* ed = new TextEditor();
    ed->SetText(active_editor->GetText());
    auto lang = active_editor->GetLanguageDefinition();
    ed->SetLanguageDefinition(lang);
    ed->SetPalette(active_editor->GetPalette());
    ed->SetTabSize(active_editor->GetTabSize());
    
    Split* split = new Split();
    split->editor = ed;
    split->editor_owned = true;
    split->is_horizontal = true;
    split->ratio = (float)(1.0 / (tab.splits.size() + 1));
    
    tab.splits.push_back(split);
    tab.active_split = (int)tab.splits.size() - 1;
}

void EditorApp::split_vertical() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    TextEditor* active_editor = get_active_editor();
    if (!active_editor) return;
    
    TextEditor* ed = new TextEditor();
    ed->SetText(active_editor->GetText());
    auto lang = active_editor->GetLanguageDefinition();
    ed->SetLanguageDefinition(lang);
    ed->SetPalette(active_editor->GetPalette());
    ed->SetTabSize(active_editor->GetTabSize());
    
    Split* split = new Split();
    split->editor = ed;
    split->editor_owned = true;
    split->is_horizontal = false;
    split->ratio = (float)(1.0 / (tab.splits.size() + 1));
    
    tab.splits.push_back(split);
    tab.active_split = (int)tab.splits.size() - 1;
}

void EditorApp::close_split() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    
    if (tab.splits.size() <= 1) return;
    
    Split* split = tab.splits[tab.active_split];
    
    // Only delete the editor if this split owns it
    if (split->editor_owned) {
        delete split->editor;
    }
    delete split;
    
    tab.splits.erase(tab.splits.begin() + tab.active_split);
    
    if (tab.active_split >= (int)tab.splits.size()) {
        tab.active_split = (int)tab.splits.size() - 1;
    }
}

void EditorApp::next_split() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    if (tab.splits.empty()) return;
    
    tab.active_split = (tab.active_split + 1) % (int)tab.splits.size();
}

void EditorApp::prev_split() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    if (tab.splits.empty()) return;
    
    tab.active_split = (tab.active_split - 1 + (int)tab.splits.size()) % (int)tab.splits.size();
}

void EditorApp::open_file_split(const std::string& path) {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    
    if (!path.empty()) {
        split_horizontal();
        open_file(path);
    }
}

void EditorApp::rotate_splits() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    if (tab.splits.size() <= 1) return;
    
    tab.active_split = (tab.active_split + 1) % (int)tab.splits.size();
}

void EditorApp::equalize_splits() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    if (tab.splits.empty()) return;
    
    float equal_ratio = 1.0f / (float)tab.splits.size();
    for (auto* split : tab.splits) {
        split->ratio = equal_ratio;
    }
}

void EditorApp::render_splits(int tab_idx) {
    if (tab_idx < 0 || tab_idx >= (int)tabs_.size()) return;
    auto& tab = tabs_[tab_idx];
    
    float scale = tab.zoom_pct / 100.0f;
    float old_scale = ImGui::GetIO().FontGlobalScale;
    ImGui::GetIO().FontGlobalScale = old_scale * scale;
    
    if (tab.splits.empty()) {
        tab.editor->Render("TextEditor");
        ImGui::GetIO().FontGlobalScale = old_scale;
        return;
    }
    
    if (tab.splits.size() == 1) {
        tab.splits[0]->editor->Render("TextEditor");
        ImGui::GetIO().FontGlobalScale = old_scale;
        return;
    }
    
    ImVec2 avail = ImGui::GetContentRegionAvail();
    
    // Determine if we have horizontal or vertical splits
    bool has_horizontal = false;
    bool has_vertical = false;
    for (auto* s : tab.splits) {
        if (s->is_horizontal) has_horizontal = true;
        else has_vertical = true;
    }
    
    if (has_horizontal || (!has_horizontal && !has_vertical)) {
        // Horizontal splits - stack top to bottom
        int n = (int)tab.splits.size();
        float total_ratio = 0;
        for (int i = 0; i < n - 1; i++) {
            total_ratio += tab.splits[i]->ratio;
        }
        
        float y = 0;
        for (int i = 0; i < n; i++) {
            auto* split = tab.splits[i];
            float h;
            if (i < n - 1) {
                h = avail.y * (split->ratio / total_ratio) * (1.0f - total_ratio);
            } else {
                h = avail.y - y;
            }
            
            ImGui::SetNextWindowPos(ImVec2(0, y));
            ImGui::SetNextWindowSize(ImVec2(avail.x, h));
            ImGui::BeginChild(("hsplit_" + std::to_string(i)).c_str(), ImVec2(avail.x, h), true, ImGuiWindowFlags_NoScrollbar);
            if (split->editor) {
                split->editor->Render(("Editor_h" + std::to_string(i)).c_str());
            }
            ImGui::EndChild();
            y += h;
            
            if (i < n - 1) {
                // Resize handle
                ImGui::InvisibleButton(("rs_" + std::to_string(i)).c_str(), ImVec2(avail.x, 4));
                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                }
                if (ImGui::IsItemActive()) {
                    ImGuiIO& io = ImGui::GetIO();
                    float delta = io.MouseDelta.y / avail.y;
                    float new_ratio = split->ratio + delta;
                    if (new_ratio < 0.2f) new_ratio = 0.2f;
                    if (new_ratio > 0.8f) new_ratio = 0.8f;
                    split->ratio = new_ratio;
                }
            }
        }
    } else {
        // Vertical splits - stack left to right
        int n = (int)tab.splits.size();
        float total_ratio = 0;
        for (int i = 0; i < n - 1; i++) {
            total_ratio += tab.splits[i]->ratio;
        }
        
        float x = 0;
        for (int i = 0; i < n; i++) {
            auto* split = tab.splits[i];
            float w;
            if (i < n - 1) {
                w = avail.x * (split->ratio / total_ratio) * (1.0f - total_ratio);
            } else {
                w = avail.x - x;
            }
            
            ImGui::SetNextWindowPos(ImVec2(x, 0));
            ImGui::SetNextWindowSize(ImVec2(w, avail.y));
            ImGui::BeginChild(("vsplit_" + std::to_string(i)).c_str(), ImVec2(w, avail.y), true, ImGuiWindowFlags_NoScrollbar);
            if (split->editor) {
                split->editor->Render(("Editor_v" + std::to_string(i)).c_str());
            }
            ImGui::EndChild();
            x += w;
            
            if (i < n - 1) {
                // Resize handle
                ImGui::SameLine();
                ImGui::InvisibleButton(("rs_" + std::to_string(i)).c_str(), ImVec2(4, avail.y));
                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
                if (ImGui::IsItemActive()) {
                    ImGuiIO& io = ImGui::GetIO();
                    float delta = io.MouseDelta.x / avail.x;
                    float new_ratio = split->ratio + delta;
                    if (new_ratio < 0.2f) new_ratio = 0.2f;
                    if (new_ratio > 0.8f) new_ratio = 0.8f;
                    split->ratio = new_ratio;
                }
            }
        }
    }
}





































