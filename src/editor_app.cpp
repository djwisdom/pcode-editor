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
    f << "  \"show_spaces\": " << (s.show_spaces ? "true" : "false") << ",\n";
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
        return content.substr(q1 + 1, q2 - q1 - 1);
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
    s.show_spaces = get_bool("show_spaces", false);
    s.tab_size = get_int("tab_size", 4);
    s.font_size = get_int("font_size", 16);
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
                s.recent_files.push_back(arr.substr(pos + 1, end - pos - 1));
                pos = end + 1;
            }
        }
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
EditorApp::EditorApp(int argc, char* argv[])
    : argc_(argc), argv_(argv) {
    new_tab();  // Start with one untitled tab
    font_size_temp_ = settings_.font_size;
    tab_size_temp_ = settings_.tab_size;
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
    
    // Apply initial font settings
    io.FontGlobalScale = settings_.font_size / 16.0f;
    font_size_temp_ = settings_.font_size;

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
}

void EditorApp::new_window() {
    // Launch a new instance of the same executable
#ifdef _WIN32
    system("start pcode-editor.exe");
#else
    system("./pcode-editor &");
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
    TextEditor* ed = tabs_[active_tab_].editor;
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
    TextEditor* ed = tabs_[active_tab_].editor;
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
    TextEditor* ed = tabs_[active_tab_].editor;
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
    TextEditor* ed = tabs_[active_tab_].editor;
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
    float scale = tabs_[tab_idx].zoom_pct / 100.0f;
    ImGui::GetIO().FontGlobalScale = scale * (settings_.font_size / 16.0f);
}

void EditorApp::toggle_status_bar() {
    settings_.show_status_bar = !settings_.show_status_bar;
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
    auto pos = tabs_[active_tab_].editor->GetCursorPosition().mLine;
    auto it = std::upper_bound(bookmarks.begin(), bookmarks.end(), pos);
    if (it != bookmarks.end()) {
        tabs_[active_tab_].editor->SetCursorPosition(TextEditor::Coordinates(*it, 0));
    } else if (!bookmarks.empty()) {
        tabs_[active_tab_].editor->SetCursorPosition(TextEditor::Coordinates(bookmarks[0], 0));
    }
}

void EditorApp::prev_bookmark() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& bookmarks = tabs_[active_tab_].bookmarks;
    if (bookmarks.empty()) return;
    auto pos = tabs_[active_tab_].editor->GetCursorPosition().mLine;
    auto it = std::lower_bound(bookmarks.begin(), bookmarks.end(), pos);
    if (it != bookmarks.begin()) {
        --it;
        tabs_[active_tab_].editor->SetCursorPosition(TextEditor::Coordinates(*it, 0));
    } else {
        tabs_[active_tab_].editor->SetCursorPosition(TextEditor::Coordinates(bookmarks.back(), 0));
    }
}

void EditorApp::clear_bookmarks() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    tabs_[active_tab_].bookmarks.clear();
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

void EditorApp::rebuild_fonts() {
    // Dynamic font change - apply immediately to all tabs
    float scale = settings_.font_size / 16.0f;
    ImGui::GetIO().FontGlobalScale = scale;
    
    // Mark all tabs as needing re-render
    for (int i = 0; i < (int)tabs_.size(); i++) {
        apply_zoom(i);
    }
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
                tabs_[active_tab_].editor->SelectAll(); 
            }
            return; 
        }
    }
    if (io.KeyCtrl && io.KeyShift && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_N)) { new_window(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_S)) { save_tab_as(active_tab_); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_W)) { close_window(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_P)) { show_cmd_palette_ = true; return; }
    }
    if (io.KeyCtrl && io.KeyAlt && !io.KeyShift) {
        if (ImGui::IsKeyPressed(ImGuiKey_S)) { save_all(); return; }
    }
    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_F3)) { find_next(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_F2)) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                auto pos = tabs_[active_tab_].editor->GetCursorPosition();
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
                tabs_[active_tab_].editor->InsertText(buf);
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

    // Full-screen dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("DockSpace", nullptr, flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderDockWindow("Editor", dockspace_id);

    render_menu_bar();
    render_editor_area();
    if (settings_.show_status_bar) render_status_bar();

    // Dialogs
    if (show_find_) render_find_dialog();
    if (show_replace_) render_replace_dialog();
    if (show_goto_) render_goto_dialog();
    if (show_font_) render_font_dialog();
    if (show_spaces_dialog_) render_spaces_dialog();
    if (show_cmd_palette_) render_command_palette();

    ImGui::End();
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
                tabs_[active_tab_].editor->Undo(); 
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Cut", "Ctrl+X")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                tabs_[active_tab_].editor->Cut(); 
            }
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                tabs_[active_tab_].editor->Copy(); 
            }
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                tabs_[active_tab_].editor->Paste(); 
            }
        }
        if (ImGui::MenuItem("Delete", "Del")) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                tabs_[active_tab_].editor->Delete(); 
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
                tabs_[active_tab_].editor->SelectAll(); 
            }
        }
        if (ImGui::MenuItem("Time/Date", "F5")) {
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                auto now = std::chrono::system_clock::now();
                auto t = std::chrono::system_clock::to_time_t(now);
                char buf[64];
                strftime(buf, sizeof(buf), "%H:%M %Y-%m-%d", localtime(&t));
                tabs_[active_tab_].editor->InsertText(buf);
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
        bool ww = settings_.word_wrap;
        if (ImGui::MenuItem("Word Wrap", nullptr, &ww)) toggle_word_wrap();
        bool ln = settings_.show_line_numbers;
        if (ImGui::MenuItem("Line Numbers", nullptr, &ln)) toggle_line_numbers();
        bool sp = settings_.show_spaces;
        if (ImGui::MenuItem("Show Spaces", nullptr, &sp)) toggle_spaces();
        ImGui::Separator();
        if (ImGui::BeginMenu("Spaces")) {
            if (ImGui::MenuItem("2 Spaces", nullptr, settings_.tab_size == 2)) set_tab_size(2);
            if (ImGui::MenuItem("4 Spaces", nullptr, settings_.tab_size == 4)) set_tab_size(4);
            if (ImGui::MenuItem("8 Spaces", nullptr, settings_.tab_size == 8)) set_tab_size(8);
            if (ImGui::MenuItem("Custom...")) { show_spaces_ = true; tab_size_temp_ = settings_.tab_size; }
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
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

    if (tabs_.empty()) {
        new_tab();
    }

    // Tab bar - only show if more than one tab
    bool show_tabs = tabs_.size() > 1;
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
                    active_tab_ = i;
                    
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

    ImGui::End();
}

void EditorApp::render_editor_with_margins() {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    TextEditor* editor = tab.editor;
    
    // Show custom gutter only for bookmarks and change history (not line numbers - TextEditor handles that)
    bool has_bookmarks = !tab.bookmarks.empty();
    bool has_changes = !tab.changed_lines.empty();
    
    if (has_bookmarks || has_changes) {
        float gutter_width = 40.0f;
        
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        
        int total_lines = editor->GetTotalLines();
        
        // Render gutter for bookmarks and change history
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.24f, 1.0f));
        if (ImGui::BeginChild("##Gutter", ImVec2(gutter_width, -1), false, ImGuiWindowFlags_NoScrollbar)) {
            for (int line = 0; line < total_lines; line++) {
                ImGui::PushID(line);
                
                // Bookmark column
                bool is_bookmarked = std::find(tab.bookmarks.begin(), tab.bookmarks.end(), line) != tab.bookmarks.end();
                if (is_bookmarked) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ""); // Checkmark symbol
                } else {
                    ImGui::Text("");
                }
                ImGui::SameLine();
                
                // Change history column (highlight modified lines)
                bool is_changed = std::find(tab.changed_lines.begin(), tab.changed_lines.end(), line) != tab.changed_lines.end();
                if (is_changed) {
                    ImGui::TextColored(ImVec4(0.3f, 0.7f, 0.3f, 1.0f), ""); // Dot symbol
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
        ImGui::PopStyleVar();
        
        ImGui::EndGroup();
    } else {
        // No gutter needed, just render editor
        editor->Render("TextEditor");
    }
}

// ============================================================================
// Status Bar
// ============================================================================
void EditorApp::render_status_bar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float height = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));

    if (ImGui::Begin("StatusBar", nullptr, flags)) {
        ImGui::PopStyleVar(2);

        if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
            auto& tab = tabs_[active_tab_];
            auto pos = tab.editor->GetCursorPosition();

            // Left: file name
            ImGui::Text("%s%s", tab.display_name.c_str(), tab.dirty ? " *" : "");
            ImGui::SameLine();

            // Right side items
            float right_start = ImGui::GetWindowWidth() - 400;
            ImGui::SetCursorPosX(right_start);

            // Line/Col
            ImGui::Text("Ln %d, Col %d", pos.mLine + 1, pos.mColumn + 1);
            ImGui::SameLine();

            // Zoom
            ImGui::Text("%d%%", tab.zoom_pct);
            ImGui::SameLine();

            // Encoding
            std::string enc = "UTF-8";
            if (!tab.file_path.empty()) {
                enc = get_file_encoding(tab.file_path);
            }
            ImGui::Text("%s", enc.c_str());
            ImGui::SameLine();

            // Line ending
            std::string le = "CRLF";
            if (!tab.file_path.empty()) {
                std::ifstream f(tab.file_path, std::ios::binary);
                if (f.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                    le = get_line_ending(content);
                }
            }
            ImGui::Text("%s", le.c_str());
            ImGui::SameLine();

            // File size
            size_t sz = 0;
            if (!tab.file_path.empty()) sz = get_file_size(tab.file_path);
            if (sz > 1024 * 1024) ImGui::Text("%.1f MB", sz / (1024.0 * 1024.0));
            else if (sz > 1024) ImGui::Text("%.1f KB", sz / 1024.0);
            else ImGui::Text("%zu B", sz);
        }

        ImGui::End();
    } else {
        ImGui::PopStyleVar(2);
    }
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
                tabs_[active_tab_].editor->SetCursorPosition(
                    TextEditor::Coordinates(line - 1, 0));
            }
            show_goto_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Go")) {
            int line = atoi(goto_line_buf_);
            if (line > 0 && active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                tabs_[active_tab_].editor->SetCursorPosition(
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
        #ifdef _WIN32
        LOGFONTW lf = {};
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfFaceName[0] = '\0';
        lf.lfPitchAndFamily = 0;
        HDC hdc = GetDC(NULL);
        EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)[](const LOGFONTW* lf, const TEXTMETRICW* tm, DWORD fontType, LPARAM lParam) -> int {
            auto* fonts = (std::vector<std::string>*)lParam;
            std::wstring name(lf->lfFaceName);
            if (!name.empty()) {
                std::string narrow(name.begin(), name.end());
                if (std::find(fonts->begin(), fonts->end(), narrow) == fonts->end()) {
                    fonts->push_back(narrow);
                }
            }
            return 1;
        }, (LPARAM)&font_list, 0);
        ReleaseDC(NULL, hdc);
        #endif
        fonts_loaded = true;
    }
    
    ImGui::OpenPopup("Font Settings");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

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
        ImGui::TextUnformatted("Changing font family requires application restart.");

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
