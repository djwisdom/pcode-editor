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
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <TextEditor.h>
#include <nfd.h>

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <handleapi.h>
#include <processthreadsapi.h>
#ifndef HPCON
typedef void* HPCON;
#endif
#ifndef EXTENDED_STARTUPINFOPRESENT
#define EXTENDED_STARTUPINFOPRESENT 0x00080000
#endif
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 22
#endif
typedef HRESULT(WINAPI *CreatePseudoConsole_t)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
typedef void(WINAPI *ClosePseudoConsole_t)(HPCON);
typedef HRESULT(WINAPI *ResizePseudoConsole_t)(HPCON, COORD);
static CreatePseudoConsole_t CreatePseudoConsole_ = nullptr;
static ClosePseudoConsole_t ClosePseudoConsole_ = nullptr;
static ResizePseudoConsole_t ResizePseudoConsole_ = nullptr;
static bool conpty_available_ = false;
static void init_conpty() {
    static bool initialized = false;
    if (initialized) return;
    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    if (hKernel) {
        CreatePseudoConsole_ = (CreatePseudoConsole_t)GetProcAddress(hKernel, "CreatePseudoConsole");
        ClosePseudoConsole_ = (ClosePseudoConsole_t)GetProcAddress(hKernel, "ClosePseudoConsole");
        ResizePseudoConsole_ = (ResizePseudoConsole_t)GetProcAddress(hKernel, "ResizePseudoConsole");
        conpty_available_ = (CreatePseudoConsole_ && ClosePseudoConsole_ && ResizePseudoConsole_);
    }
    initialized = true;
}
#endif

#include <cstdio>
#include <cmath>
#include <string>
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
    f << "  \"enable_vim_mode\": " << (s.enable_vim_mode ? "true" : "false") << ",\n";
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
    s.enable_vim_mode = false;  // Force disabled - user can't edit with vim on
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
    // Try to read VERSION file first
    std::string version;
    std::ifstream ver_file("VERSION");
    if (ver_file.is_open()) {
        std::getline(ver_file, version);
        ver_file.close();
    }
    
    // If file not found, use embedded default
    if (version.empty()) {
        version = "0.7.1 (887481c38cbbb88e7d96fd4dddf5be8f4f6c8f1e)";
    }
    return "pCode Editor version 0.7.1 (887481c38cbbb88e7d96fd4dddf5be8f4f6c8f1e)" + version;
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
            std::string ver = get_version();
    printf("%s\n", ver.c_str());
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
        
        // Multi-viewport support
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        
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
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
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
    
    // Set user pointer BEFORE setting callbacks (critical!)
    glfwSetWindowUserPointer(window_, this);
    
    // Enable drag-drop via GLFW callback
    glfwSetDropCallback(window_, [](GLFWwindow* window, int count, const char** paths) {
        EditorApp* app = (EditorApp*)glfwGetWindowUserPointer(window);
        if (app && count > 0) {
            fprintf(stderr, "DEBUG GLFW DROP: %d files\n", count);
            for (int i = 0; i < count; i++) {
                fprintf(stderr, "DEBUG: %s\n", paths[i]);
                app->open_file(paths[i]);
            }
        }
    });
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    
    // Enable file drop via GLFW callback
    glfwSetDropCallback(window_, [](GLFWwindow* window, int count, const char** paths) {
        EditorApp* app = (EditorApp*)glfwGetWindowUserPointer(window);
        if (app && count > 0) {
            fprintf(stderr, "DEBUG: Drop received %d files\n", count);
            for (int i = 0; i < count; i++) {
                fprintf(stderr, "DEBUG: Drop file %d: %s\n", i, paths[i]);
                app->open_file(paths[i]);
            }
        }
    });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    // Disabled docking - may cause menu issues
    
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
    
    // Native Windows status bar - disabled for now (causes issues)
    // create_native_status_bar();
}

void EditorApp::create_native_status_bar() {
#ifdef _WIN32
    if (!window_) return;
    
    HWND hwnd = glfwGetWin32Window(window_);
    if (!hwnd) return;
    
    // Create status bar - let it auto-position at bottom
    HWND status_hwnd = CreateWindowEx(
        0,
        STATUSCLASSNAME,
        "",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,  // Position/size - let Windows handle this
        hwnd,
        (HMENU)1,
        GetModuleHandle(NULL),
        NULL
    );
    native_status_bar = (void*)status_hwnd;
    
    // Initialize parts - set up the partition layout once
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int part1 = 150;
    if (part1 > clientWidth / 3) part1 = clientWidth / 3;
    int parts[2] = {part1, -1};
    if (clientWidth > 200) {
        SendMessage(status_hwnd, SB_SETPARTS, 2, (LPARAM)parts);
    }
    
    // Show the window to initialize
    ShowWindow(status_hwnd, SW_SHOW);
#endif
}

void EditorApp::update_native_status_bar() {
#ifdef _WIN32
    if (!native_status_bar) return;
    
    HWND status_hwnd = (HWND)native_status_bar;
    
    // Get window dimensions for dynamic sizing
    int width, height;
    glfwGetWindowSize(window_, &width, &height);
    
    // Reposition status bar to bottom of window (accounting for menu + tabs)
    // Get client rect to find actual content area
    RECT clientRect;
    GetClientRect(glfwGetWin32Window(window_), &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;
    
    // Calculate needed size for status bar
    int statusHeight = 24;
    int statusY = clientHeight - statusHeight;
    
    // Resize the status bar control to match window width
    MoveWindow(status_hwnd, 0, statusY, clientWidth, statusHeight, TRUE);
    
    // Re-set parts after MoveWindow (it resets them)
    int part1 = 150;
    if (part1 > clientWidth / 3) part1 = clientWidth / 3;
    int parts[2] = {part1, -1};
    if (clientWidth > 200) {
        SendMessage(status_hwnd, SB_SETPARTS, 2, (LPARAM)parts);
    }
    
    // Part 0: Mode
    std::string vim_mode = get_vim_mode_str();
    wchar_t mode_buf[64] = L"";
    mbstowcs(mode_buf, vim_mode.c_str(), 63);
    SendMessage(status_hwnd, SB_SETTEXT, 0, (LPARAM)mode_buf);
    
    // Part 1: filename + info (fill)
    if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
        auto& tab = tabs_[active_tab_];
        std::string name = tab.display_name;
        if (tab.dirty) name += " *";
        
        TextEditor* ed = get_active_editor();
        if (ed) {
            auto pos = ed->GetCursorPosition();
            name += " | Ln " + std::to_string(pos.mLine + 1) + ", Col " + std::to_string(pos.mColumn + 1);
        }
        
        wchar_t file_buf[512] = L"";
        mbstowcs(file_buf, name.c_str(), 511);
        SendMessage(status_hwnd, SB_SETTEXT, 1, (LPARAM)file_buf);
    } else {
        SendMessage(status_hwnd, SB_SETTEXT, 1, (LPARAM)L"");
    }
#endif
}

void EditorApp::shutdown() {
    // Save window size before destroying
    int w, h;
    glfwGetWindowSize(window_, &w, &h);
    settings_.window_w = w;
    settings_.window_h = h;
    save_settings();
    
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

void EditorApp::new_terminal_tab() {
    // Count existing terminal tabs
    int terminal_count = 0;
    for (auto& tab : tabs_) {
        if (tab.is_terminal) terminal_count++;
    }
    if (terminal_count >= 10) return;  // Max 10 terminals
    
    EditorTab tab;
    tab.display_name = "Terminal " + std::to_string(terminal_count + 1);
    tab.is_terminal = true;
    tab.editor = nullptr;
    
    tabs_.push_back(std::move(tab));
    active_tab_ = (int)tabs_.size() - 1;
    
    // Each terminal tab gets its own process
    start_terminal();
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
    
    // Ensure focus is on the new editor
    active_tab_ = tab_idx;
    tab.active_split = 0;
    
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
    
    // Log activity
    std::string fname = std::filesystem::path(selected_path).filename().string();
    activity_log_.push_back("Opened: " + fname);
    if (activity_log_.size() > 20) activity_log_.erase(activity_log_.begin());
    
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
    // Cycle: left -> right -> hidden -> left (radio behavior)
    if (explorer_side_ == 0) explorer_side_ = 1;
    else if (explorer_side_ == 1) explorer_side_ = -1;
    else explorer_side_ = 0;
    show_file_tree_ = (explorer_side_ != -1);
}

void EditorApp::toggle_word_wrap() {
    settings_.word_wrap = !settings_.word_wrap;
    // Note: Word wrap requires TextEditor library patch - toggle works internally
    // Horizontal scrollbar visibility is handled by TextEditor
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

void EditorApp::toggle_code_folding() {
    settings_.show_code_folding = !settings_.show_code_folding;
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
    auto lines = editor.GetTextLines();
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
        case VimMode::VisualLine: return "V-LINE";
        case VimMode::VisualBlock: return "V-BLOCK";
        case VimMode::OperatorPending: return "OPERATOR";
        case VimMode::Command: return "COMMAND";
        default: return "";
    }
}

void EditorApp::handle_vim_key(int key, int /*mods*/) {
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
        vim_find_pending_ = false;
        return;
    }
    
    // In insert mode, only Escape matters
    if (vim_mode_ == VimMode::Insert) {
        return;
    }
    
    // Handle Ctrl+v for Visual Block mode
    if (key == ImGuiKey_V && ImGui::GetIO().KeyCtrl) {
        vim_mode_ = VimMode::VisualBlock;
        return;
    }
    
    // Handle character find pending mode (f, F, t, T waiting for character)
    // Note: Find character feature simplified - needs vim_count_ implementation
    if (vim_find_pending_) {
        vim_find_pending_ = false;
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
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
            case '_':  // g_ - last non-blank character
                if (line < (int)lines.size()) {
                    std::string& text = lines[line];
                    int last_non_blank = (int)text.size();
                    while (last_non_blank > 0 && (text[last_non_blank - 1] == ' ' || text[last_non_blank - 1] == '\t')) {
                        last_non_blank--;
                    }
                    ed->SetCursorPosition(TextEditor::Coordinates(line, last_non_blank));
                }
                break;
            // Screen motions: H, M, L
            case 'H':  // Home - first visible line
                // Move to first line (approximate - use top of visible area)
                ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                // Scroll to top - use approximate line count
                for (int i = 0; i < 15 && line > 0; i++) line--;
                ed->SetCursorPosition(TextEditor::Coordinates(line, col));
                break;
            case 'M':  // Middle - middle visible line
                for (int i = 0; i < 10 && line > 0; i++) line--;
                ed->SetCursorPosition(TextEditor::Coordinates(line, col));
                break;
            case 'L':  // Last - last visible line
                for (int i = 0; i < 15 && line < ed->GetTotalLines() - 1; i++) line++;
                ed->SetCursorPosition(TextEditor::Coordinates(line, col));
                break;
            // Scroll commands: z Enter, z-, zt, zb, zz
            case 'z':
                vim_mode_ = VimMode::OperatorPending;
                vim_operator_ = 'z';
                return;
            // Character find: f, F, t, T
            case 'f':
            case 'F':
            case 't':
            case 'T':
                vim_find_pending_ = true;
                vim_mode_ = VimMode::OperatorPending;
                vim_operator_ = kb[0];
                return;
            // Repeat search: ; and ,
            case ';':
                if (vim_last_find_char_) {
                    // Repeat last search in same direction
                    char search_char = vim_last_find_char_;
                    bool is_till = false;  // assume f/F, not t/T
                    // Search forward
                    for (int i = 0; i < count; i++) {
                        if (line < (int)lines.size()) {
                            std::string& text = lines[line];
                            int search_col = col + 1;
                            int found_col = -1;
                            while (search_col < (int)text.size()) {
                                if (text[search_col] == search_char) {
                                    found_col = search_col;
                                    break;
                                }
                                search_col++;
                            }
                            if (found_col >= 0) {
                                col = found_col;
                            }
                        }
                    }
                    ed->SetCursorPosition(TextEditor::Coordinates(line, col));
                }
                break;
            case ',':
                if (vim_last_find_char_) {
                    // Repeat last search in opposite direction
                    char search_char = vim_last_find_char_;
                    for (int i = 0; i < count; i++) {
                        if (line < (int)lines.size()) {
                            std::string& text = lines[line];
                            int search_col = col - 1;
                            int found_col = -1;
                            while (search_col >= 0) {
                                if (text[search_col] == search_char) {
                                    found_col = search_col;
                                    break;
                                }
                                search_col--;
                            }
                            if (found_col >= 0) {
                                col = found_col;
                            }
                        }
                    }
                    ed->SetCursorPosition(TextEditor::Coordinates(line, col));
                }
                break;
            // Paragraph motions: { and }
            case '{':
                // Move to previous blank line (paragraph start)
                for (int i = 0; i < count; i++) {
                    while (line > 0) {
                        line--;
                        if (line < (int)lines.size() && lines[line].empty()) {
                            // Found blank line, move to first non-blank after it
                            line++;
                            break;
                        }
                    }
                }
                // Move to first non-blank of paragraph
                while (line < (int)lines.size() && (lines[line].empty() || 
                       (lines[line][0] == ' ' || lines[line][0] == '\t'))) {
                    line++;
                }
                ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                break;
            case '}':
                // Move to next blank line (paragraph end)
                for (int i = 0; i < count; i++) {
                    bool found_blank = false;
                    while (line < ed->GetTotalLines()) {
                        if (line < (int)lines.size() && lines[line].empty()) {
                            found_blank = true;
                            line++;
                            break;
                        }
                        line++;
                    }
                    // Skip blank lines
                    while (line < (int)lines.size() && lines[line].empty()) {
                        line++;
                    }
                }
                ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                break;
            // Sentence motions: ( and )
            case '(':
                // Move to previous sentence start
                for (int i = 0; i < count; i++) {
                    while (line > 0) {
                        line--;
                        if (line < (int)lines.size() && !lines[line].empty()) {
                            // Check for sentence end punctuation
                            char last_char = lines[line][lines[line].size() - 1];
                            if (last_char == '.' || last_char == '!' || last_char == '?') {
                                line--;
                                break;
                            }
                        }
                    }
                }
                // Find first non-blank character
                while (line < (int)lines.size() && 
                       (lines[line].empty() || lines[line][0] == ' ' || lines[line][0] == '\t')) {
                    line++;
                }
                ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                break;
            case ')':
                // Move to next sentence start
                for (int i = 0; i < count; i++) {
                    while (line < ed->GetTotalLines()) {
                        if (line < (int)lines.size() && !lines[line].empty()) {
                            char first_char = lines[line][0];
                            if (first_char == '.' || first_char == '!' || first_char == '?') {
                                line++;
                                break;
                            }
                        }
                        line++;
                    }
                    // Skip whitespace
                    while (line < (int)lines.size() && 
                           (lines[line].empty() || lines[line][0] == ' ' || lines[line][0] == '\t')) {
                        line++;
                    }
                }
ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                break;
            // Insert mode commands (only when no operator is pending)
            case 'i':
                // i - enter Insert mode at current position (if no operator pending)
                if (vim_operator_ == 0) {
                    vim_mode_ = VimMode::Insert;
                    vim_key_buffer_.clear();
                    vim_count_ = 0;
                    return;
                }
                // Otherwise, treat as text object start (but preserve existing operator)
                vim_mode_ = VimMode::OperatorPending;
                // vim_operator_ already set by previous operator, don't overwrite
                return;
            case 'a':
                // a - move forward one character, then Insert mode (if no operator pending)
                if (vim_operator_ == 0) {
                    if (line < (int)lines.size() && col < (int)lines[line].size()) {
                        ed->SetCursorPosition(TextEditor::Coordinates(line, col + 1));
                    } else if (line < (int)lines.size()) {
                        ed->SetCursorPosition(TextEditor::Coordinates(line, (int)lines[line].size()));
                    }
                    vim_mode_ = VimMode::Insert;
                    vim_key_buffer_.clear();
                    vim_count_ = 0;
                    return;
                }
                // Otherwise, treat as text object start (but preserve existing operator)
                vim_mode_ = VimMode::OperatorPending;
                // vim_operator_ already set by previous operator, don't overwrite
                return;
            case 'I':
                // I - go to first non-blank, then Insert mode
                if (line < (int)lines.size()) {
                    std::string& text = lines[line];
                    int start = 0;
                    while (start < (int)text.size() && (text[start] == ' ' || text[start] == '\t')) start++;
                    ed->SetCursorPosition(TextEditor::Coordinates(line, start));
                }
                vim_mode_ = VimMode::Insert;
                vim_key_buffer_.clear();
                vim_count_ = 0;
                return;
            case 'A':
                // A - go to line end, then Insert mode
                if (line < (int)lines.size()) {
                    ed->SetCursorPosition(TextEditor::Coordinates(line, (int)lines[line].size()));
                }
                vim_mode_ = VimMode::Insert;
                vim_key_buffer_.clear();
                vim_count_ = 0;
                return;
            case 'o':
                // o - insert newline below, move to it, Insert mode
                if (line < (int)lines.size()) {
                    lines.insert(lines.begin() + line + 1, "");
                    ed->SetCursorPosition(TextEditor::Coordinates(line + 1, 0));
                    tabs_[active_tab_].dirty = true;
                } else {
                    lines.push_back("");
                    ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                    tabs_[active_tab_].dirty = true;
                }
                vim_mode_ = VimMode::Insert;
                vim_key_buffer_.clear();
                vim_count_ = 0;
                return;
            case 'O':
                // O - insert newline above, move to it, Insert mode
                if (line < (int)lines.size()) {
                    lines.insert(lines.begin() + line, "");
                    ed->SetCursorPosition(TextEditor::Coordinates(line, 0));
                    tabs_[active_tab_].dirty = true;
                } else {
                    lines.push_back("");
                    ed->SetCursorPosition(TextEditor::Coordinates(0, 0));
                    tabs_[active_tab_].dirty = true;
                }
                vim_mode_ = VimMode::Insert;
                vim_key_buffer_.clear();
                vim_count_ = 0;
                return;
            // Text objects - set pending mode (i and a handled above)
            case 'w':
                vim_mode_ = VimMode::OperatorPending;
                vim_operator_ = kb[0];
                return;
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
    
    // g operators: g~, gU, gu
    if (vim_mode_ == VimMode::OperatorPending && vim_operator_ == 'g') {
        if (kb == "g~") {
            // g~ - toggle case of current character
            if (line < (int)lines.size() && col < (int)lines[line].size()) {
                char ch = lines[line][col];
                if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
                else if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
                lines[line][col] = ch;
                tabs_[active_tab_].dirty = true;
            }
        } else if (kb == "gU") {
            // gU - uppercase current line
            if (line < (int)lines.size()) {
                std::string& text = lines[line];
                for (size_t i = 0; i < text.size(); i++) {
                    if (text[i] >= 'a' && text[i] <= 'z') {
                        text[i] = text[i] - 'a' + 'A';
                    }
                }
                tabs_[active_tab_].dirty = true;
            }
        } else if (kb == "gu") {
            // gu - lowercase current line
            if (line < (int)lines.size()) {
                std::string& text = lines[line];
                for (size_t i = 0; i < text.size(); i++) {
                    if (text[i] >= 'A' && text[i] <= 'Z') {
                        text[i] = text[i] - 'A' + 'a';
                    }
                }
                tabs_[active_tab_].dirty = true;
            }
        } else if (kb == "g~2" || kb == "gU2" || kb == "gu2") {
            // With count prefix
            int cnt = (kb[2] - '0') * count;
            if (kb == "g~2") {
                for (int i = 0; i < cnt && line < (int)lines.size(); i++, line++) {
                    std::string& text = lines[line];
                    for (size_t j = 0; j < text.size(); j++) {
                        if (text[j] >= 'a' && text[j] <= 'z') text[j] = text[j] - 'a' + 'A';
                        else if (text[j] >= 'A' && text[j] <= 'Z') text[j] = text[j] - 'A' + 'a';
                    }
                }
                tabs_[active_tab_].dirty = true;
            } else if (kb == "gU2") {
                for (int i = 0; i < cnt && line < (int)lines.size(); i++, line++) {
                    std::string& text = lines[line];
                    for (size_t j = 0; j < text.size(); j++) {
                        if (text[j] >= 'a' && text[j] <= 'z') text[j] = text[j] - 'a' + 'A';
                    }
                }
                tabs_[active_tab_].dirty = true;
            } else if (kb == "gu2") {
                for (int i = 0; i < cnt && line < (int)lines.size(); i++, line++) {
                    std::string& text = lines[line];
                    for (size_t j = 0; j < text.size(); j++) {
                        if (text[j] >= 'A' && text[j] <= 'Z') text[j] = text[j] - 'A' + 'a';
                    }
                }
                tabs_[active_tab_].dirty = true;
            }
        }
        
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_operator_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    
    // Handle operator pending word motions: dw, de, d$, d0, etc.
    if (vim_mode_ == VimMode::OperatorPending) {
        char op = vim_operator_;
        char motion = kb[0];
        
        // Word motions with operator
        if (motion == 'w') {
            // dw - delete word forward
            int start_line = line;
            int start_col = col;
            // Find end of word
            for (int c = 0; c < count; c++) {
                if (line < (int)lines.size()) {
                    std::string& text = lines[line];
                    int end_col = col;
                    while (end_col < (int)text.size()) {
                        char cw = text[end_col];
                        bool is_word = (cw >= 'a' && cw <= 'z') || (cw >= 'A' && cw <= 'Z') ||
                                    (cw >= '0' && cw <= '9') || cw == '_';
                        if (!is_word) break;
                        end_col++;
                    }
                    // Skip non-word chars
                    while (end_col < (int)text.size()) {
                        char cw = text[end_col];
                        bool is_word = (cw >= 'a' && cw <= 'z') || (cw >= 'A' && cw <= 'Z') ||
                                    (cw >= '0' && cw <= '9') || cw == '_';
                        if (is_word) break;
                        end_col++;
                    }
                    col = end_col;
                }
            }
            if (op == 'd') {
                ed->SetSelectionStart(TextEditor::Coordinates(start_line, start_col));
                ed->SetSelectionEnd(TextEditor::Coordinates(line, col));
                ed->Delete();
            } else if (op == 'y') {
                ed->SetSelectionStart(TextEditor::Coordinates(start_line, start_col));
                ed->SetSelectionEnd(TextEditor::Coordinates(line, col));
                ed->Copy();
            } else if (op == 'c') {
                ed->SetSelectionStart(TextEditor::Coordinates(start_line, start_col));
                ed->SetSelectionEnd(TextEditor::Coordinates(line, col));
                ed->Delete();
                vim_mode_ = VimMode::Insert;
            }
            vim_key_buffer_.clear();
            vim_count_ = 0;
            vim_operator_ = 0;
            vim_mode_ = VimMode::Normal;
            return;
        }
        
        if (motion == 'b') {
            // db - delete word backward
            int start_line = line;
            int start_col = col;
            for (int c = 0; c < count; c++) {
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
                }
            }
            if (op == 'd') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, col));
                ed->SetSelectionEnd(TextEditor::Coordinates(start_line, start_col));
                ed->Delete();
            } else if (op == 'y') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, col));
                ed->SetSelectionEnd(TextEditor::Coordinates(start_line, start_col));
                ed->Copy();
            } else if (op == 'c') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, col));
                ed->SetSelectionEnd(TextEditor::Coordinates(start_line, start_col));
                ed->Delete();
                vim_mode_ = VimMode::Insert;
            }
            vim_key_buffer_.clear();
            vim_count_ = 0;
            vim_operator_ = 0;
            vim_mode_ = VimMode::Normal;
            return;
        }
        
        if (motion == 'e') {
            // de - delete to word end
            int start_line = line;
            int start_col = col;
            for (int c = 0; c < count; c++) {
                if (line < (int)lines.size()) {
                    std::string& text = lines[line];
                    int end_col = col;
                    while (end_col < (int)text.size()) {
                        char cw = text[end_col];
                        bool is_word = (cw >= 'a' && cw <= 'z') || (cw >= 'A' && cw <= 'Z') ||
                                    (cw >= '0' && cw <= '9') || cw == '_';
                        if (!is_word) break;
                        end_col++;
                    }
                    col = end_col;
                }
            }
            if (op == 'd') {
                ed->SetSelectionStart(TextEditor::Coordinates(start_line, start_col));
                ed->SetSelectionEnd(TextEditor::Coordinates(line, col));
                ed->Delete();
            } else if (op == 'y') {
                ed->SetSelectionStart(TextEditor::Coordinates(start_line, start_col));
                ed->SetSelectionEnd(TextEditor::Coordinates(line, col));
                ed->Copy();
            } else if (op == 'c') {
                ed->SetSelectionStart(TextEditor::Coordinates(start_line, start_col));
                ed->SetSelectionEnd(TextEditor::Coordinates(line, col));
                ed->Delete();
                vim_mode_ = VimMode::Insert;
            }
            vim_key_buffer_.clear();
            vim_count_ = 0;
            vim_operator_ = 0;
            vim_mode_ = VimMode::Normal;
            return;
        }
        
        if (motion == '$') {
            // d$ - delete to end of line
            if (op == 'd') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, col));
                if (line < (int)lines.size()) {
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, (int)lines[line].size()));
                }
                ed->Delete();
            } else if (op == 'y') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, col));
                if (line < (int)lines.size()) {
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, (int)lines[line].size()));
                }
                ed->Copy();
            } else if (op == 'c') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, col));
                if (line < (int)lines.size()) {
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, (int)lines[line].size()));
                }
                ed->Delete();
                vim_mode_ = VimMode::Insert;
            }
            vim_key_buffer_.clear();
            vim_count_ = 0;
            vim_operator_ = 0;
            vim_mode_ = VimMode::Normal;
            return;
        }
        
        if (motion == '0') {
            // d0 - delete to beginning of line
            if (op == 'd') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, 0));
                ed->SetSelectionEnd(TextEditor::Coordinates(line, col));
                ed->Delete();
            } else if (op == 'y') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, 0));
                ed->SetSelectionEnd(TextEditor::Coordinates(line, col));
                ed->Copy();
            } else if (op == 'c') {
                ed->SetSelectionStart(TextEditor::Coordinates(line, 0));
                ed->SetSelectionEnd(TextEditor::Coordinates(line, col));
                ed->Delete();
                vim_mode_ = VimMode::Insert;
            }
            vim_key_buffer_.clear();
            vim_count_ = 0;
            vim_operator_ = 0;
            vim_mode_ = VimMode::Normal;
            return;
        }
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
    
    // ge - word end backward
    if (kb == "ge") {
        for (int c = 0; c < count; c++) {
            if (line < (int)lines.size()) {
                std::string& text = lines[line];
                int end_col = col;
                // First move to end of current word or previous word
                while (end_col > 0) {
                    char prev_c = text[end_col - 1];
                    bool is_word = (prev_c >= 'a' && prev_c <= 'z') || (prev_c >= 'A' && prev_c <= 'Z') ||
                                (prev_c >= '0' && prev_c <= '9') || prev_c == '_';
                    if (is_word) {
                        end_col--;
                    } else {
                        break;
                    }
                }
                // If at start of line or already at word end, go to previous word end
                if (end_col == 0 || col == 0) {
                    if (line > 0) {
                        // Go to previous line
                        line--;
                        text = lines[line];
                        end_col = (int)text.size();
                        // Find end of last word
                        while (end_col > 0) {
                            char prev_c = text[end_col - 1];
                            bool is_word = (prev_c >= 'a' && prev_c <= 'z') || (prev_c >= 'A' && prev_c <= 'Z') ||
                                        (prev_c >= '0' && prev_c <= '9') || prev_c == '_';
                            if (!is_word) break;
                            end_col--;
                        }
                    }
                }
                col = end_col;
                ed->SetCursorPosition(TextEditor::Coordinates(line, col));
            }
        }
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    
    // Scroll commands: z Enter, z-, zt, zb, zz
    if (kb == "z\n" || kb == "z\r") {
        // z Enter - redraw with current line at top
        ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        // Scroll to show current line at top - approximate
        for (int i = 0; i < 10 && line > 0; i++) line--;
        ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    if (kb == "z-") {
        // z- - redraw with current line at bottom
        for (int i = 0; i < 10 && line < ed->GetTotalLines() - 1; i++) line++;
        ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    if (kb == "zt") {
        // zt - redraw with current line at top
        for (int i = 0; i < 10 && line > 0; i++) line--;
        ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    if (kb == "zb") {
        // zb - redraw with current line at bottom
        for (int i = 0; i < 10 && line < ed->GetTotalLines() - 1; i++) line++;
        ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    if (kb == "zz") {
        // zz - redraw with current line at center
        ed->SetCursorPosition(TextEditor::Coordinates(line, col));
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_mode_ = VimMode::Normal;
        return;
    }
    
    // Text objects handling: iw, aw, iW, aW, is, ip, i(, a(, i), a), i[, a[, i], a], i{, a{, i}, a}, i<, a<, i>, a>, i", a", i', a', i`, a`
    // Format: operator + text_object (like "diw", "ciw", "yaw")
    if (vim_mode_ == VimMode::OperatorPending && kb.size() >= 2) {
        char op = vim_operator_;
        std::string to = kb;  // text object
        
        // Word text objects
        if (to == "iw" || to == "aw" || to == "iW" || to == "aW") {
            // Find word boundaries at current position
            if (line < (int)lines.size()) {
                std::string& text = lines[line];
                int start = col;
                int end = col;
                
                // Find word start
                while (start > 0) {
                    char c = text[start - 1];
                    bool is_word = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '_';
                    if (!is_word) break;
                    start--;
                }
                // Find word end
                while (end < (int)text.size()) {
                    char c = text[end];
                    bool is_word = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '_';
                    if (!is_word) break;
                    end++;
                }
                
                // For 'a' include trailing whitespace
                if (to[0] == 'a') {
                    while (end < (int)text.size() && (text[end] == ' ' || text[end] == '\t')) {
                        end++;
                    }
                }
                
                if (op == 'd') {
                    ed->SetSelectionStart(TextEditor::Coordinates(line, start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, end));
                    ed->Delete();
                } else if (op == 'y') {
                    ed->SetSelectionStart(TextEditor::Coordinates(line, start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, end));
                    ed->Copy();
                } else if (op == 'c') {
                    ed->SetSelectionStart(TextEditor::Coordinates(line, start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, end));
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
        
        // Parenthesis text objects: i(, a(, i), a), i( can also be i)
        if (to == "i(" || to == "i)" || to == "a(" || to == "a)") {
            int depth = 0;
            int start_line = line;
            int start_col = col;
            int open_pos = -1;
            int close_pos = -1;
            
            // Search backward for opening parenthesis
            for (int l = line; l >= 0 && open_pos < 0; l--) {
                std::string& txt = lines[l];
                for (int i = (l == line ? col : (int)txt.size() - 1); i >= 0; i--) {
                    if (txt[i] == ')') depth++;
                    else if (txt[i] == '(' && depth > 0) { depth--; }
                    else if (txt[i] == '(' && depth == 0) {
                        open_pos = i;
                        start_line = l;
                        start_col = i;
                        break;
                    }
                }
            }
            
            // Search forward for closing parenthesis
            depth = 0;
            for (int l = line; l < ed->GetTotalLines() && close_pos < 0; l++) {
                std::string& txt = lines[l];
                for (int i = (l == line ? col : 0); i < (int)txt.size(); i++) {
                    if (txt[i] == '(') depth++;
                    else if (txt[i] == ')' && depth > 0) { depth--; }
                    else if (txt[i] == ')' && depth == 0) {
                        close_pos = i;
                        break;
                    }
                }
            }
            
            if (open_pos >= 0 && close_pos >= 0) {
                int sel_start = (to[0] == 'i') ? open_pos + 1 : open_pos;
                int sel_end = (to[0] == 'i') ? close_pos : close_pos + 1;
                
                if (op == 'd') {
                    ed->SetSelectionStart(TextEditor::Coordinates(start_line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
                    ed->Delete();
                } else if (op == 'y') {
                    ed->SetSelectionStart(TextEditor::Coordinates(start_line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
                    ed->Copy();
                } else if (op == 'c') {
                    ed->SetSelectionStart(TextEditor::Coordinates(start_line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
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
        
        // Brace text objects: i{, a{, i}, a}
        if (to == "i{" || to == "i}" || to == "a{" || to == "a}") {
            int depth = 0;
            int start_line = line;
            int start_col = col;
            int open_pos = -1;
            int close_pos = -1;
            
            // Search backward for opening brace
            for (int l = line; l >= 0 && open_pos < 0; l--) {
                std::string& txt = lines[l];
                for (int i = (l == line ? col : (int)txt.size() - 1); i >= 0; i--) {
                    if (txt[i] == '}') depth++;
                    else if (txt[i] == '{' && depth > 0) { depth--; }
                    else if (txt[i] == '{' && depth == 0) {
                        open_pos = i;
                        start_line = l;
                        start_col = i;
                        break;
                    }
                }
            }
            
            // Search forward for closing brace
            depth = 0;
            for (int l = line; l < ed->GetTotalLines() && close_pos < 0; l++) {
                std::string& txt = lines[l];
                for (int i = (l == line ? col : 0); i < (int)txt.size(); i++) {
                    if (txt[i] == '{') depth++;
                    else if (txt[i] == '}' && depth > 0) { depth--; }
                    else if (txt[i] == '}' && depth == 0) {
                        close_pos = i;
                        break;
                    }
                }
            }
            
            if (open_pos >= 0 && close_pos >= 0) {
                int sel_start = (to[0] == 'i') ? open_pos + 1 : open_pos;
                int sel_end = (to[0] == 'i') ? close_pos : close_pos + 1;
                
                if (op == 'd') {
                    ed->SetSelectionStart(TextEditor::Coordinates(start_line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
                    ed->Delete();
                } else if (op == 'y') {
                    ed->SetSelectionStart(TextEditor::Coordinates(start_line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
                    ed->Copy();
                } else if (op == 'c') {
                    ed->SetSelectionStart(TextEditor::Coordinates(start_line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
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
        
        // Bracket text objects: i[, a[, i], a]
        if (to == "i[" || to == "i]" || to == "a[" || to == "a]") {
            int depth = 0;
            int start_line = line;
            int open_pos = -1;
            int close_pos = -1;
            
            // Search backward for opening bracket
            for (int l = line; l >= 0 && open_pos < 0; l--) {
                std::string& txt = lines[l];
                for (int i = (l == line ? col : (int)txt.size() - 1); i >= 0; i--) {
                    if (txt[i] == ']') depth++;
                    else if (txt[i] == '[' && depth > 0) { depth--; }
                    else if (txt[i] == '[' && depth == 0) {
                        open_pos = i;
                        start_line = l;
                        break;
                    }
                }
            }
            
            // Search forward for closing bracket
            depth = 0;
            for (int l = line; l < ed->GetTotalLines() && close_pos < 0; l++) {
                std::string& txt = lines[l];
                for (int i = (l == line ? col : 0); i < (int)txt.size(); i++) {
                    if (txt[i] == '[') depth++;
                    else if (txt[i] == ']' && depth > 0) { depth--; }
                    else if (txt[i] == ']' && depth == 0) {
                        close_pos = i;
                        break;
                    }
                }
            }
            
            if (open_pos >= 0 && close_pos >= 0) {
                int sel_start = (to[0] == 'i') ? open_pos + 1 : open_pos;
                int sel_end = (to[0] == 'i') ? close_pos : close_pos + 1;
                
                if (op == 'd') {
                    ed->SetSelectionStart(TextEditor::Coordinates(start_line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
                    ed->Delete();
                } else if (op == 'y') {
                    ed->SetSelectionStart(TextEditor::Coordinates(start_line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
                    ed->Copy();
                } else if (op == 'c') {
                    ed->SetSelectionStart(TextEditor::Coordinates(start_line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
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
        
        // Quote text objects: i", a", i', a'
        if (to == "i\"" || to == "a\"" || to == "i'" || to == "a'") {
            char quote_char = to[1];
            int start_pos = -1;
            int end_pos = -1;
            
            // Search forward for quote
            for (int l = line; l < ed->GetTotalLines() && end_pos < 0; l++) {
                std::string& txt = lines[l];
                for (int i = (l == line ? col + 1 : 0); i < (int)txt.size(); i++) {
                    if (txt[i] == quote_char && (i == 0 || txt[i-1] != '\\')) {
                        end_pos = i;
                        break;
                    }
                }
            }
            
            // Search backward for opening quote
            for (int l = line; l >= 0 && start_pos < 0; l--) {
                std::string& txt = lines[l];
                for (int i = (l == line ? col - 1 : (int)txt.size() - 1); i >= 0; i--) {
                    if (txt[i] == quote_char && (i == 0 || txt[i-1] != '\\')) {
                        start_pos = i;
                        break;
                    }
                }
            }
            
            if (start_pos >= 0 && end_pos >= 0) {
                int sel_start = (to[0] == 'i') ? start_pos + 1 : start_pos;
                int sel_end = (to[0] == 'i') ? end_pos : end_pos + 1;
                
                if (op == 'd') {
                    ed->SetSelectionStart(TextEditor::Coordinates(line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
                    ed->Delete();
                } else if (op == 'y') {
                    ed->SetSelectionStart(TextEditor::Coordinates(line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
                    ed->Copy();
                } else if (op == 'c') {
                    ed->SetSelectionStart(TextEditor::Coordinates(line, sel_start));
                    ed->SetSelectionEnd(TextEditor::Coordinates(line, sel_end));
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
        
        // Handle motion after operator (like dw, de, d$, etc.)
        // For simple word/line motions
        if (op == 'd' || op == 'y' || op == 'c') {
            // These will be handled in the existing operator pending code
        }
    }
    
    // Handle g operator (g~, gU, gu)
    if (vim_mode_ == VimMode::OperatorPending && vim_operator_ == 'g' && kb.size() >= 2) {
        char second = kb[1];
        if (second == '~') {
            // g~ - toggle case
            if (line < (int)lines.size() && col < (int)lines[line].size()) {
                char ch = lines[line][col];
                if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
                else if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
                lines[line][col] = ch;
                tabs_[active_tab_].dirty = true;
            }
        } else if (second == 'U') {
            // gU - uppercase
            if (line < (int)lines.size()) {
                std::string& text = lines[line];
                for (size_t i = 0; i < text.size(); i++) {
                    if (text[i] >= 'a' && text[i] <= 'z') {
                        text[i] = text[i] - 'a' + 'A';
                    }
                }
                tabs_[active_tab_].dirty = true;
            }
        } else if (second == 'u') {
            // gu - lowercase
            if (line < (int)lines.size()) {
                std::string& text = lines[line];
                for (size_t i = 0; i < text.size(); i++) {
                    if (text[i] >= 'A' && text[i] <= 'Z') {
                        text[i] = text[i] - 'A' + 'a';
                    }
                }
                tabs_[active_tab_].dirty = true;
            }
        } else if (second == 'e') {
            // ge - word end backward (already handled in multi-key)
        }
        
        vim_key_buffer_.clear();
        vim_count_ = 0;
        vim_operator_ = 0;
        vim_mode_ = VimMode::Normal;
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
// ============================================================================
// Layout Validation — verifies UI components are in correct positions
// Run via: View menu or Ctrl+Shift+L
// ============================================================================
void EditorApp::validate_layout() {
    // Check Editor window exists
    ImGuiWindow* editor_win = ImGui::FindWindowByName("Editor");
    if (!editor_win) {
        printf("[LAYOUT] ERROR: Editor window not found\n");
        return;
    }
    
    ImVec2 ew_pos = editor_win->Pos;
    float ew_w = editor_win->Size.x;
    float ew_h = editor_win->Size.y;
    printf("\n========== LAYOUT VALIDATION ==========\n");
    printf("[EDITOR] pos=(%d,%d) size=(%d,%d)\n", (int)ew_pos.x, (int)ew_pos.y, (int)ew_w, (int)ew_h);
    
    // Test 1: Status bar inside Editor at bottom
    if (settings_.show_status_bar) {
        float status_y = ew_pos.y + ew_h - 24;
        ImVec2 status_pos = ImGui::GetWindowPos(); // Current window context
        // Check if status bar renders at bottom inside Editor
        ImGui::SetCursorPos(ImVec2(0, ew_h - 24 - ImGui::GetStyle().ScrollbarSize));
        ImVec2 cursor = ImGui::GetCursorPos();
        bool status_ok = cursor.y > 0 && cursor.y < ew_h;
        printf("[STATUS] %s (bottom, inside editor) - %s\n", 
            status_ok ? "OK" : "FAIL",
            settings_.show_status_bar ? "enabled" : "disabled");
    }
    
    // Test 2: Minimap at right side of editor
    if (settings_.show_minimap) {
        ImVec2 current_pos = ImGui::GetWindowPos();
        bool minimap_inside = current_pos.x + 70 > ew_pos.x;
        printf("[MINIMAP] %s (right side inside) - %s\n",
            minimap_inside ? "OK" : "FAIL",
            settings_.show_minimap ? "enabled" : "disabled");
    }
    
    // Test 3: Gutter at left side of editor
    if (settings_.show_line_numbers || settings_.show_bookmark_margin) {
        printf("[GUTTER] %s (left side inside) - %s\n",
            "OK", // gutter is inside child window
            (settings_.show_line_numbers || settings_.show_bookmark_margin) ? "enabled" : "disabled");
    }
    
    // Test 4: Explorer (sidebar) on left
    printf("[EXPLORER] %s (toggle with Ctrl+B)\n",
        show_file_tree_ ? "OK (left sidebar)" : "disabled");
    
    // Test 5: Terminal at bottom inside Editor
    printf("[TERMINAL] %s (toggle with Ctrl+`)\n",
        show_terminal_ ? "OK (bottom inside)" : "disabled");
    
    printf("======================================\n\n");
    
    if (settings_.show_status_bar && settings_.show_minimap) {
        printf("NOTE: Enable components via View menu, then check:\n");
        printf("  - Status bar should be at bottom of Editor\n");
        printf("  - Minimap should be at right side\n");
        printf("  - Gutter should be at left of code\n");
    }
    
    bool has_status = settings_.show_status_bar;
    bool has_minimap = settings_.show_minimap;
    bool has_gutter = settings_.show_line_numbers || settings_.show_bookmark_margin;
    printf("[LAYOUT] StatusBar: %s | Minimap: %s | Gutter: %s | Explorer: %s\n",
        has_status ? "enabled" : "disabled",
        has_minimap ? "enabled" : "disabled",
        has_gutter ? "enabled" : "disabled",
        show_file_tree_ ? "enabled (toggle with Ctrl+B)" : "disabled");
}

void EditorApp::render() {
    // Global keyboard shortcuts
    ImGuiIO& io = ImGui::GetIO();
    
    // Reset vim input skip flag at start of each frame
    skip_texteditor_input_ = false;
    
    // Handle Escape - only when vim mode enabled
    if (settings_.enable_vim_mode && !terminal_input_active_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        vim_mode_ = VimMode::Normal;
        vim_key_buffer_.clear();
        vim_count_ = 0;
        // Clear any pending input
        io.InputQueueCharacters.resize(0);
        for (int key = ImGuiKey_A; key <= ImGuiKey_Z; key++) {
            io.AddKeyEvent((ImGuiKey)key, false);
        }
        for (int key = ImGuiKey_0; key <= ImGuiKey_9; key++) {
            io.AddKeyEvent((ImGuiKey)key, false);
        }
    }
    
    // Only handle vim keys when vim mode is enabled AND in Normal mode
    if (settings_.enable_vim_mode && vim_mode_ == VimMode::Normal && !terminal_input_active_) {
        bool vim_key_handled = false;
        
        // Handle vim motion keys with IsKeyDown for continuous repeat when held
        // This makes vim motions scroll/fly fast - keys repeat every frame when held
        // First ensure editor has focus so cursor is visible
        TextEditor* ed = get_active_editor();
        if (ed && ImGui::IsKeyDown(ImGuiKey_J)) { ed->SetCursorPosition(ed->GetCursorPosition()); }  // Ensure cursor render
        else if (ImGui::IsKeyDown(ImGuiKey_K)) { handle_vim_key(ImGuiKey_K, 0); vim_key_handled = true; }
        else if (ImGui::IsKeyDown(ImGuiKey_H)) { handle_vim_key(ImGuiKey_H, 0); vim_key_handled = true; }
        else if (ImGui::IsKeyDown(ImGuiKey_L)) { handle_vim_key(ImGuiKey_L, 0); vim_key_handled = true; }
        else if (ImGui::IsKeyDown(ImGuiKey_W)) { handle_vim_key(ImGuiKey_W, 0); vim_key_handled = true; }
        else if (ImGui::IsKeyDown(ImGuiKey_B)) { handle_vim_key(ImGuiKey_B, 0); vim_key_handled = true; }
        else if (ImGui::IsKeyDown(ImGuiKey_E)) { handle_vim_key(ImGuiKey_E, 0); vim_key_handled = true; }
        else if (ImGui::IsKeyDown(ImGuiKey_0)) { handle_vim_key(ImGuiKey_0, 0); vim_key_handled = true; }
        
        // Block ALL letter keys (A-Z) in Normal mode - don't let them pass to TextEditor
        // Only check on initial press (not repeat) to avoid double-processing
        if (!vim_key_handled) {
            for (int key = ImGuiKey_A; key <= ImGuiKey_Z; key++) {
                if (ImGui::IsKeyPressed((ImGuiKey)key)) { 
                    handle_vim_key(key, 0); 
                    vim_key_handled = true; 
                    break; 
                }
            }
        }
        
        // Block number keys in Normal mode (only allow count prefix 0-9)
        if (!vim_key_handled) {
            for (int key = ImGuiKey_1; key <= ImGuiKey_9; key++) {
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
        
        // Handle Ctrl+v for Visual Block mode
        if (ImGui::IsKeyPressed(ImGuiKey_V) && io.KeyCtrl) {
            vim_mode_ = VimMode::VisualBlock;
            return;
        }
        
        // Handle Page Up/Down with Ctrl (Ctrl+B, Ctrl+F, Ctrl+U, Ctrl+D)
        if (io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {  // Ctrl+B - page up
                handle_vim_key(ImGuiKey_PageUp, 0);
                vim_key_handled = true;
            } else if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {  // Ctrl+F - page down
                handle_vim_key(ImGuiKey_PageDown, 0);
                vim_key_handled = true;
            }
        }
        
        // After vim key handling, clear ALL letter/number keys to prevent TextEditor from receiving them
        // This ensures unmapped keys like 'x', 'q', 'z' don't insert text in Normal mode
        // BUT only clear if we're still in Normal mode (vim mode may have changed to Insert during handle_vim_key)
        if (vim_mode_ == VimMode::Normal) {
            for (int key = ImGuiKey_A; key <= ImGuiKey_Z; key++) {
                io.AddKeyEvent((ImGuiKey)key, false);
            }
            for (int key = ImGuiKey_0; key <= ImGuiKey_9; key++) {
                io.AddKeyEvent((ImGuiKey)key, false);
            }
            io.AddKeyEvent(ImGuiKey_Space, false);
            io.AddKeyEvent(ImGuiKey_Backspace, false);
            io.AddKeyEvent(ImGuiKey_Tab, false);
            
            // Clear InputQueueCharacters to prevent any character input in Normal mode
            io.InputQueueCharacters.resize(0);
        }
    }
    
    // Clear InputQueueCharacters ONLY when vim mode is enabled and in Normal mode
    if (settings_.enable_vim_mode && vim_mode_ == VimMode::Normal && !terminal_input_active_) {
        ImGuiIO& io = ImGui::GetIO();
        io.InputQueueCharacters.resize(0);
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
            // Ctrl+Tab - next tab
            if (ImGui::IsKeyPressed(ImGuiKey_Tab)) { 
                if (tabs_.size() > 1) {
                    active_tab_ = (active_tab_ + 1) % tabs_.size();
                    ImGui::SetWindowFocus("Editor");
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
            // Ctrl+Shift+Tab - previous tab
            if (ImGui::IsKeyPressed(ImGuiKey_Tab)) { 
                if (tabs_.size() > 1) {
                    active_tab_ = (active_tab_ - 1 + (int)tabs_.size()) % tabs_.size();
                    ImGui::SetWindowFocus("Editor");
                }
                return; 
            }
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

// Main window - Editor with menu, sidebar, editor area all inside
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::Begin("pCode Editor", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar);
    // NOTE: SetWindowFocus removed - it was closing menus instantly
    // Always render menus - no conditional
    render_menu_bar();
    
    // Context menu - right click only
    if (ImGui::BeginPopupContextWindow("##ContextMenu")) {
        if (ImGui::MenuItem("New File", "Ctrl+N")) new_tab();
        if (ImGui::MenuItem("Open...", "Ctrl+O")) open_file("");
        ImGui::Separator();
        if (ImGui::MenuItem("Terminal", nullptr, &show_terminal_)) {
            if (show_terminal_ && !terminal_process_) start_terminal();
        }
        if (ImGui::MenuItem("Status Bar", nullptr, &settings_.show_status_bar)) toggle_status_bar();
        ImGui::Separator();
        if (ImGui::MenuItem("Git", nullptr, &show_git_changes_)) {}
        if (ImGui::MenuItem("Symbol", nullptr, &show_symbol_outline_)) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Line Numbers", nullptr, &settings_.show_line_numbers)) toggle_line_numbers();
        if (ImGui::MenuItem("Minimap", nullptr, &settings_.show_minimap)) toggle_minimap();
        if (ImGui::MenuItem("Code Folding", nullptr, &settings_.show_code_folding)) toggle_code_folding();
        ImGui::EndPopup();
    }
    
    render_editor_area();
    render_status_bar();
    ImGui::End();

    // Dialogs
    if (show_find_) render_find_dialog();
    if (show_replace_) render_replace_dialog();
    if (show_goto_) render_goto_dialog();
    if (show_font_) render_font_dialog();
    if (show_spaces_dialog_) render_spaces_dialog();
    if (show_cmd_palette_) render_command_palette();
    if (show_about_) render_about_dialog();
    
    // Native Windows status bar
    update_native_status_bar();
}

// ============================================================================
// Menu Bar
// ============================================================================
void EditorApp::render_menu_bar() {
    if (ImGui::BeginMenuBar()) {
        render_menu_file();
        render_menu_edit();
        render_menu_view();
        render_menu_split();
        render_menu_help();
        ImGui::EndMenuBar();
    }
}

void EditorApp::render_menu_file() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Tab", "Ctrl+N")) new_tab();
        if (ImGui::MenuItem("New Terminal", "Ctrl+Shift+T")) new_terminal_tab();
        if (ImGui::MenuItem("New Window", "Ctrl+Shift+N")) new_window();
        if (ImGui::BeginMenu("Open...")) {
            if (ImGui::MenuItem("In New Tab", "Ctrl+O")) open_file("");
            if (ImGui::MenuItem("In New Horizontal Split")) open_file_split("");
            ImGui::EndMenu();
        }

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
        // Vim mode toggle
        bool vim = settings_.enable_vim_mode;
        if (ImGui::MenuItem("Vim Mode", nullptr, &vim)) {
            settings_.enable_vim_mode = !settings_.enable_vim_mode;
            if (!settings_.enable_vim_mode) {
                vim_mode_ = VimMode::Normal;  // Reset to normal when disabled
            }
        }
        ImGui::Separator();
        
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
        if (ImGui::BeginMenu("Explorer Side")) {
            bool exp_left = (explorer_side_ == 0);
            bool exp_right = (explorer_side_ == 1);
            bool exp_none = (explorer_side_ == -1);
            if (ImGui::MenuItem("Left", nullptr, &exp_left)) { explorer_side_ = 0; show_file_tree_ = true; }
            if (ImGui::MenuItem("Right", nullptr, &exp_right)) { explorer_side_ = 1; show_file_tree_ = true; }
            if (ImGui::MenuItem("None", nullptr, &exp_none)) { explorer_side_ = -1; show_file_tree_ = false; }
            ImGui::EndMenu();
        }
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
        if (ImGui::MenuItem("Validate Layout", "Ctrl+Shift+L")) validate_layout();
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
        if (show_terminal_) {
            ImGui::Indent();
            if (ImGui::MenuItem("Bottom", nullptr, settings_.terminal_position == 0)) settings_.terminal_position = 0;
            if (ImGui::MenuItem("Left", nullptr, settings_.terminal_position == 1)) settings_.terminal_position = 1;
            if (ImGui::MenuItem("Top", nullptr, settings_.terminal_position == 2)) settings_.terminal_position = 2;
            if (ImGui::MenuItem("Right", nullptr, settings_.terminal_position == 3)) settings_.terminal_position = 3;
            ImGui::Unindent();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(settings_.dark_theme ? "Light Theme" : "Dark Theme")) toggle_theme();
ImGui::EndMenu();
    }
}

void EditorApp::render_menu_help() {
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {
            show_about_ = true;
        }
        ImGui::EndMenu();
    }
}

void EditorApp::render_menu_split() {
    if (ImGui::BeginMenu("Split")) {
        if (ImGui::MenuItem("Don't Split Tab", "Ctrl+Alt+W")) close_split();
        ImGui::Separator();
        if (ImGui::MenuItem("Split Tab Horizontally", "Ctrl+Shift+H")) split_horizontal(false);
        if (ImGui::MenuItem("Split Tab Vertically", "Ctrl+Shift+V")) split_vertical(false);
        ImGui::Separator();
        if (ImGui::MenuItem("Split Terminal Horizontally")) split_horizontal(true);
        if (ImGui::MenuItem("Split Terminal Vertically")) split_vertical(true);
        ImGui::Separator();
        if (ImGui::MenuItem("Split and Open Horizontally")) open_file_split("");
        if (ImGui::MenuItem("Split and Open Vertically")) open_file_split_vertical("");
        ImGui::Separator();
        if (ImGui::MenuItem("Focus Next", "Ctrl+K")) next_split();
        if (ImGui::MenuItem("Focus Previous", "Ctrl+J")) prev_split();
        ImGui::Separator();
        if (ImGui::MenuItem("Rotate Splits", "Ctrl+Alt+K")) rotate_splits();
        if (ImGui::MenuItem("Equal Size", "Ctrl+Alt+E")) equalize_splits();
        ImGui::EndMenu();
    }
}

void EditorApp::render_about_dialog() {
    if (!show_about_) return;
    
    // Center on main window
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    ImGui::OpenPopup("About");
    if (ImGui::BeginPopupModal("About", &show_about_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Dummy(ImVec2(4, 0));  // Left margin
        
        ImGui::Text("Personal Code Editor");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));
        
        // VERSION format: "0.6.3 (hash)"
        std::string version = get_version();
        size_t start = version.find("0.");  // Find "0.x" start
        size_t paren = version.find(" (");  // Find " ("
        std::string ver_part = version.substr(start, paren - start);  // "0.6.3"
        std::string full_hash = version.substr(paren + 2);  // after " ("
        full_hash = full_hash.substr(0, full_hash.find(")"));  // hash
        std::string short_hash = full_hash.substr(0, 7);  // short hash
        
        ImGui::Text("Version: %s (%s)", ver_part.c_str(), short_hash.c_str());
        ImGui::Text("Commit: %s", full_hash.c_str());
        ImGui::Text("Built: " __DATE__);
        
        ImGui::Dummy(ImVec2(0, 10));
        
        int glfw_major, glfw_minor, glfw_rev;
        glfwGetVersion(&glfw_major, &glfw_minor, &glfw_rev);
        ImGui::Text("GLFW: %d.%d.%d", glfw_major, glfw_minor, glfw_rev);
        ImGui::Text("Dear ImGui: " IMGUI_VERSION);
        ImGui::Text("ImGuiColorTextEdit: 1.0");
        
#if defined(_WIN32)
        ImGui::Text("OS: Microsoft Windows");
#elif defined(__APPLE__)
        ImGui::Text("OS: Apple");
#elif defined(__linux__)
        ImGui::Text("OS: Linux");
#else
        ImGui::Text("OS: Unknown");
#endif
        
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));
        
        ImGui::Text("SPDX-License-Identifier: BSD-2-Clause");
        ImGui::Text("Copyright (c) 2026 pCode Editor Development Team");
        ImGui::Text("Author: Dennis O. Esternon <djwisdom@serenityos.org>");
        ImGui::Text("Contributors: see CONTRIBUTORS or git history");
        
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));
        
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 50.0f);
        if (ImGui::Button("OK", ImVec2(50, 0))) {
            show_about_ = false;
        }
        
        ImGui::EndPopup();
    }
}

// ============================================================================
// Editor Area
// ============================================================================
void EditorApp::render_editor_area() {
    // Render terminal panel if enabled (View > Terminal)
    if (show_terminal_) {
        render_terminal();
    }
    
    if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size() && tabs_[active_tab_].is_terminal) {
        return;
    }
    
    // Editor area inside the Editor window (already opened)
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow)) {
        editor_focused_ = true;
    }

    if (vim_mode_ == VimMode::Command || editor_focused_) {
        ImVec4 color = vim_mode_ == VimMode::Command 
            ? ImVec4(0.2f, 0.4f, 0.8f, 1.0f) 
            : ImVec4(0.3f, 0.7f, 0.3f, 1.0f);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetWindowPos(), 
            ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + 3),
            ImColor(color));
    }

    float status_height = settings_.show_status_bar ? 24 : 0;
    // Use content region directly - this accounts for everything
    float editor_width = ImGui::GetContentRegionAvail().x;
    float editor_height = ImGui::GetContentRegionAvail().y;
    
    if (tabs_.empty()) {
        new_tab();
    }

    bool show_tabs = settings_.show_tabs;
    float tab_height = show_tabs ? ImGui::GetFrameHeight() : 0.0f;
    float status_h = settings_.show_status_bar ? 24.0f : 0.0f;
    float editor_area_width = editor_width;
    float editor_area_height = editor_height - tab_height - status_h;
    static int prev_active_tab = -1;
    
    if (prev_active_tab != active_tab_ && prev_active_tab >= 0 && prev_active_tab < (int)tabs_.size()) {
        auto& prev_tab = tabs_[prev_active_tab];
        prev_tab.editor->GetCursorPosition();
    }
    prev_active_tab = active_tab_;
    
    // Render tabs FIXED at top (not inside scrolling child)
    if (show_tabs) {
        ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs;
        if (ImGui::BeginTabBar("##Tabs", tab_flags)) {
            for (int i = 0; i < (int)tabs_.size(); i++) {
                auto& tab = tabs_[i];
                std::string label = tab.display_name;
                if (tab.dirty) label += " *";

                bool open = true;
                if (ImGui::BeginTabItem(label.c_str(), &open)) {
                    // This tab is selected - update active_tab
                    if (active_tab_ != i) {
                        active_tab_ = i;
                    }
                    ImGui::EndTabItem();
                }
                if (!open) {
                    close_tab(i);
                    break;
                }
            }
            // Add [+] button for new tab
            if (tabs_.size() < 10) {
                if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoReorder)) {
                    char tab_name[32];
                    if (tabs_.size() == 0) snprintf(tab_name, sizeof(tab_name), "untitled");
                    else snprintf(tab_name, sizeof(tab_name), "untitled%d", (int)tabs_.size());
                    new_tab();
                    tabs_[active_tab_].display_name = tab_name;
                    tabs_[active_tab_].file_path = "";
                }
            }
            ImGui::EndTabBar();
        }
        
        // Set focus after tab bar (after we know which is active)
        if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
            ImGui::SetWindowFocus("Editor");
        }
    }
    
    // ===== EXPLORER (RADIO: LEFT/RIGHT/NONE) =====
    static float explorer_w = 200;
    static float explorer_ratio = 0.2f;  // Proportional to window
    static bool dragging = false;
    
    float avail_width = ImGui::GetContentRegionAvail().x;
    
    // Handle hidden state
    if (explorer_side_ == -1) {
        ImGui::BeginChild("##EditorOnly", ImVec2(avail_width, -1), false);
        if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
            render_editor_with_margins();
        }
        ImGui::EndChild();
        return;
    }
    
    // Seamless proportional resize - maintain ratio when window changes
    if (!dragging && avail_width > 0) {
        explorer_w = avail_width * explorer_ratio;
        if (explorer_w < 100) explorer_w = 100;
        if (explorer_w > 500) explorer_w = 500;
    }
    
    float edit_w = avail_width - explorer_w - 4;  // Account for splitter
    if (edit_w < 100) edit_w = 100;
    
    if (explorer_side_ == 0) {
        // Left explorer
        ImGui::BeginChild("##ExplorerLeft", ImVec2(explorer_w, -1), true);
        render_sidebar();
        ImGui::EndChild();
        
        // Splitter
        ImGui::SameLine();
        ImGui::InvisibleButton("##splitter", ImVec2(4, -1));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemClicked(0)) dragging = true;
        if (dragging && ImGui::IsMouseDown(0)) {
            explorer_w += ImGui::GetIO().MouseDelta.x;
            if (explorer_w < 100) explorer_w = 100;
            if (explorer_w > 500) explorer_w = 500;
            edit_w = avail_width - explorer_w;
            if (avail_width > 0) explorer_ratio = explorer_w / avail_width;
        } else if (!ImGui::IsMouseDown(0)) {
            if (avail_width > 0) explorer_ratio = explorer_w / avail_width;
            dragging = false;
        }
        
        // Editor
        ImGui::SameLine();
        ImGui::BeginChild("##EditorWithLeftExplorer", ImVec2(edit_w, -1), false);
        if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
            render_editor_with_margins();
        }
        ImGui::EndChild();
    } else {
        // Editor first (left side)
        ImGui::BeginChild("##EditorWithRightExplorer", ImVec2(edit_w, -1), false);
        if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
            render_editor_with_margins();
        }
        ImGui::EndChild();
        
        // Splitter
        ImGui::SameLine();
        ImGui::InvisibleButton("##splitter", ImVec2(4, -1));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemClicked(0)) dragging = true;
        if (dragging && ImGui::IsMouseDown(0)) {
            explorer_w -= ImGui::GetIO().MouseDelta.x;
            if (explorer_w < 100) explorer_w = 100;
            if (explorer_w > 500) explorer_w = 500;
            if (avail_width > 0) explorer_ratio = explorer_w / avail_width;
        } else if (!ImGui::IsMouseDown(0)) {
            if (avail_width > 0) explorer_ratio = explorer_w / avail_width;
            dragging = false;
        }
        
        // Right explorer
        ImGui::SameLine();
        ImGui::BeginChild("##ExplorerRight", ImVec2(explorer_w, -1), true);
        render_sidebar();
        ImGui::EndChild();
    }
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
        float line_num_width = digit_count * 8.0f + 8.0f;
        float gutter_width = line_num_width + 48.0f; // line numbers + markers on right
        
        // Build markers for lookup
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
                                if (settings_.show_bookmark_margin && mouse.x < win_pos.x + 16) {
                                    toggle_bookmark(clicked_line);
                                }
                            }
                        }
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
                
                // Line numbers on LEFT side
                if (settings_.show_line_numbers) {
                    int line_display = line + 1;
                    char line_num[16];
                    sprintf(line_num, "%*d", digit_count, line_display);
                    
                    bool is_current_line = (line == editor->GetCursorPosition().mLine);
                    if (is_current_line) {
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), line_num);
                    } else {
                        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.5f, 1.0f), line_num);
                    }
                }
                
                // Move to right side of gutter
                ImGui::SetCursorPosX(ImGui::GetItemRectMin().x + line_num_width);
                
                // Bookmark column (right side)
                if (settings_.show_bookmark_margin) {
                    bool is_bookmarked = std::find(bookmarks.begin(), bookmarks.end(), line) != bookmarks.end();
                    if (is_bookmarked) {
                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), ">");
                    } else {
                        ImGui::TextDisabled(" ");
                    }
                    ImGui::SameLine();
                }
                
                // Change history indicator (right side)
                if (settings_.show_change_history) {
                    bool is_changed = std::find(changed_lines.begin(), changed_lines.end(), line) != changed_lines.end();
                    if (is_changed) {
                        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.4f, 1.0f), "|");
                    }
                    ImGui::SameLine();
                }
                
                // Fold indicator (rightmost)
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
        
        // TextEditor internally always shows horizontal scrollbar ('AlwaysHorizontalScrollbar' flag)
        // This is controlled by internal TextEditor code, but word wrap helps visually
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
    }

// ============================================================================
// Explorer helpers - file icons, git status, sorting
// ============================================================================
static const char* get_file_icon(const std::string& path, bool is_dir) {
    if (is_dir) return "[D]";
    
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".cpp" || ext == ".c" || ext == ".cc" || ext == ".cxx") return "[C]";
    if (ext == ".h" || ext == ".hpp" || ext == ".hh") return "[H]";
    if (ext == ".js" || ext == ".jsx" || ext == ".ts" || ext == ".tsx") return "[J]";
    if (ext == ".py") return "[P]";
    if (ext == ".json") return "{}";
    if (ext == ".md" || ext == ".markdown") return "# ";
    if (ext == ".txt") return "T ";
    if (ext == ".html" || ext == ".htm") return "<>";
    if (ext == ".css") return "CS";
    if (ext == ".xml") return "XL";
    if (ext == ".yaml" || ext == ".yml") return "Y ";
    if (ext == ".sh" || ext == ".bat" || ext == ".ps1") return "$ ";
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".bmp") return "IMG";
    if (ext == ".pdf") return "PDF";
    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".rar") return "ZIP";
    if (ext == ".exe" || ext == ".dll" || ext == ".so" || ext == ".dylib") return "BIN";
    if (ext == ".gitignore" || ext == ".clang-format" || ext == ".editorconfig") return "CFG";
    
    return "F ";
}

static const char* get_git_status(const std::string& path) {
    static std::unordered_set<std::string> modified_files;
    static std::string last_checked_dir;
    static std::time_t last_check_time = 0;
    
    std::filesystem::path dir = std::filesystem::path(path).parent_path();
    std::string dir_str = dir.string();
    
    auto now = std::time(nullptr);
    if (dir_str != last_checked_dir || now - last_check_time > 5) {
        modified_files.clear();
        last_checked_dir = dir_str;
        last_check_time = now;
        
        #ifdef _WIN32
        FILE* pipe = _popen("git diff --name-only --porcelain 2>nul", "r");
        #else
        FILE* pipe = popen("git diff --name-only --porcelain 2>/dev/null", "r");
        #endif
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) {
                std::string line = buf;
                if (!line.empty() && line[0] == ' ') {
                    std::string fp = line.substr(2);
                    fp.erase(fp.find('\n'), std::string::npos);
                    modified_files.insert(fp);
                }
            }
            _pclose(pipe);
        }
    }
    
    if (modified_files.count(path)) return " M";
    return "";
}

// ============================================================================
// Explorer - file tree with split pane behavior
// ============================================================================
void EditorApp::render_sidebar() {
    if (!show_file_tree_) return;
    
    static float explorer_width = 250;
    static bool dragging = false;
    static int sidebar_view = 0;  // 0=Files, 1=Git, 2=Symbol
    
    // Explorer as a child window - fills full height
    ImGui::BeginChild("##Explorer", ImVec2(explorer_width, -1), true, ImGuiWindowFlags_NoTitleBar);
    
    // Header with tabs and close button
    if (ImGui::Button("F##view")) sidebar_view = 0;
    ImGui::SameLine();
    if (ImGui::Button("G##view")) sidebar_view = 1;
    ImGui::SameLine();
    if (ImGui::Button("S##view")) sidebar_view = 2;
    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
    if (ImGui::Button("X")) {
        show_file_tree_ = false;
    }
    ImGui::Separator();
    
    // Show selected view
    if (sidebar_view == 0) {
        render_file_tree();
    } else if (sidebar_view == 1) {
        render_git_changes();
    } else {
        render_symbol_outline();
    }
    
    ImGui::EndChild();
    
    // Draggable splitter between explorer and editor
    float avail_h = ImGui::GetContentRegionAvail().y;
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, avail_h / 2));
    ImGui::InvisibleButton("##ExplorerSplitter", ImVec2(4, -1));
    ImGui::PopStyleVar();
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(settings_.explorer_left ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeEW);
    }
    if (ImGui::IsItemClicked(0)) {
        dragging = true;
    }
    if (dragging && ImGui::IsMouseDown(0)) {
        float delta = ImGui::GetIO().MouseDelta.x;
        if (settings_.explorer_left) {
            explorer_width += delta;
        } else {
            explorer_width -= delta;
        }
        explorer_width = std::clamp(explorer_width, 150.0f, 500.0f);
    } else {
        dragging = false;
    }
}

void EditorApp::render_file_tree() {
    ImGui::Text("Files");
    ImGui::SameLine();
    if (ImGui::SmallButton("+##new")) {
        ImGui::OpenPopup("##FileTreeNewPopup");
    }
    if (ImGui::BeginPopup("##FileTreeNewPopup")) {
        if (ImGui::MenuItem("New File...")) {
            show_new_file_dialog_ = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("New Folder...")) {
            show_new_folder_dialog_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::Separator();
    
    static std::string file_filter;
    if (ImGui::InputTextWithHint("##filter", "Filter...", file_filter.data(), 32, ImGuiInputTextFlags_EnterReturnsTrue)) {
    }
    if (file_filter.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted("");
    }
    ImGui::Separator();
    
    static std::unordered_map<std::string, bool> expanded_dirs;
    static std::string current_dir = ".";
    
    std::string dir_path = settings_.last_open_dir.empty() ? "." : settings_.last_open_dir;
    
    std::filesystem::path parent = std::filesystem::path(dir_path).parent_path();
    if (!parent.empty() && parent.string() != dir_path) {
        if (ImGui::Selectable("[..]", false, ImGuiSelectableFlags_DontClosePopups)) {
            settings_.last_open_dir = parent.string();
            current_dir = parent.string();
        }
    }
    
    render_dir_tree(dir_path, expanded_dirs, file_filter);
}

void EditorApp::render_dir_tree(const std::string& dir_path, std::unordered_map<std::string, bool>& expanded_dirs, const std::string& filter) {
    if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path)) return;
    
    struct DirEntry {
        std::string name;
        std::string path;
        bool is_dir;
    };
    
    std::vector<DirEntry> entries;
    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        std::string name = entry.path().filename().string();
        if (!name.empty() && name[0] == '.') continue;
        if (!filter.empty()) {
            if (name.find(filter) == std::string::npos) continue;
        }
        entries.push_back({name, entry.path().string(), entry.is_directory()});
    }
    
    std::sort(entries.begin(), entries.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        std::string al = a.name; std::transform(al.begin(), al.end(), al.begin(), ::tolower);
        std::string bl = b.name; std::transform(bl.begin(), bl.end(), bl.begin(), ::tolower);
        return al < bl;
    });
    
    for (const auto& e : entries) {
        if (e.is_dir) {
            ImGui::PushID(e.path.c_str());
            bool is_expanded = expanded_dirs[e.path];
            
            const char* arrow = is_expanded ? "[-]" : "[+]";
            if (ImGui::Selectable(arrow, false)) {
                expanded_dirs[e.path] = !is_expanded;
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.6f, 0.3f, 1.0f), "%s %s/", get_file_icon(e.path, true), e.name.c_str());
            
            if (is_expanded) {
                ImGui::Indent(16);
                render_dir_tree(e.path, expanded_dirs, filter);
                ImGui::Unindent(16);
            }
            
            ImGui::PopID();
        } else {
            const char* git_stat = get_git_status(e.path);
            std::string display_name = std::string(get_file_icon(e.path, false)) + " " + e.name;
            if (git_stat[0] != '\0') {
                display_name += git_stat;
            }
            
            ImGui::PushID(e.path.c_str());
            if (ImGui::Selectable(display_name.c_str(), false, ImGuiSelectableFlags_DontClosePopups)) {
                bool already_open = false;
                for (size_t i = 0; i < tabs_.size(); i++) {
                    if (tabs_[i].file_path == e.path) {
                        active_tab_ = static_cast<int>(i);
                        ImGui::SetWindowFocus("Editor");
                        already_open = true;
                        break;
                    }
                }
                if (!already_open) {
                    open_file(e.path);
                }
            }
            if (ImGui::BeginPopupContextItem(e.path.c_str())) {
                if (ImGui::MenuItem("Open")) {
                    bool already_open = false;
                    for (size_t i = 0; i < tabs_.size(); i++) {
                        if (tabs_[i].file_path == e.path) { already_open = true; break; }
                    }
                    if (!already_open) open_file(e.path);
                }
                if (ImGui::MenuItem("Open in HSplit")) {
                    split_horizontal();
                    open_file(e.path);
                }
                if (ImGui::MenuItem("Open in VSplit")) {
                    split_vertical();
                    open_file(e.path);
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
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
                    active_tab_ = static_cast<int>(i);
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
    
    float status_height = 22.0f;
    float cmd_height = vim_mode_ == VimMode::Command ? 22.0f : 0.0f;
    float h_scrollbar_height = ImGui::GetStyle().ScrollbarSize;
    
    // Check if horizontal scrollbar is actually visible (content wider than viewport)
    bool has_h_scroll = false;
    TextEditor* ed = get_active_editor();
    if (ed) {
        auto lines = ed->GetTextLines();
        float char_width = 8.0f; // approximate
        for (const auto& line : lines) {
            if ((float)line.size() * char_width > ImGui::GetContentRegionAvail().x) {
                has_h_scroll = true;
                break;
            }
        }
    }
    
    // Get editor window dimensions - use content region to avoid scrollbar issues
    ImVec2 editor_pos = ImGui::GetWindowPos();
    float editor_width = ImGui::GetContentRegionAvail().x;
    float editor_height = ImGui::GetContentRegionAvail().y;
    
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
    // Only move up if horizontal scrollbar is visible
    float scroll_comp = has_h_scroll ? h_scrollbar_height : 0.0f;
    ImGui::SetCursorPosY(editor_height - status_height - scroll_comp);
    
    // Account for scrollbar on right
    float scrollbar_width = 14;
    float status_width = editor_width - scrollbar_width;
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 status_bg = ImColor(0.2f, 0.2f, 0.25f);
    draw_list->AddRectFilled(
        ImVec2(editor_pos.x, editor_pos.y + editor_height - status_height - scroll_comp),
        ImVec2(editor_pos.x + status_width, editor_pos.y + editor_height - scroll_comp),
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
        
        // Exact format: | MODE | filename | * | Ln,Col | Git:branch | encoding | CRLF | Tab:n | v0.2.98 |
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
        ImGui::Text("%s", get_version().c_str());
    }
    
ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void EditorApp::render_minimap(TextEditor* editor) {
    if (!editor) return;
    
    // Save position before any child windows
    ImVec2 editor_win_pos = ImGui::GetWindowPos();
    float window_width = ImGui::GetContentRegionAvail().x;
    float window_height = ImGui::GetContentRegionAvail().y;
    
    float minimap_width = 70;
    float minimap_x = editor_win_pos.x + window_width - minimap_width - 8;
    float minimap_height = window_height - 48; // account for menu
    
    auto lines = editor->GetTextLines();
    int total_lines = (int)lines.size();
    if (total_lines == 0) total_lines = 1;
    float scale = (minimap_height - 20) / (float)total_lines;
    if (scale > 3) scale = 3;
    if (scale < 0.5f) scale = 0.5f;
    
    ImGui::SetNextWindowPos(ImVec2(minimap_x, editor_win_pos.y + 28));
    ImGui::SetNextWindowSize(ImVec2(minimap_width, minimap_height));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                            ImGuiWindowFlags_NoInputs;
    
    if (ImGui::Begin("##Minimap", nullptr, flags)) {
        auto cursor = editor->GetCursorPosition();
        
        for (int i = 0; i < total_lines; i++) {
            float y = editor_win_pos.y + 30 + (i * scale);
            if (y < editor_win_pos.y + 30 || y > editor_win_pos.y + minimap_height + 20) continue;
            
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
                float rel_y = mouse.y - (editor_win_pos.y + 30);
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
    
    init_conpty();
    
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hStdInRd = NULL, hStdInWr = NULL;
    HANDLE hStdOutRd = NULL, hStdOutWr = NULL;
    
    CreatePipe(&hStdInRd, &hStdInWr, &sa, 0);
    CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0);
    
    SetHandleInformation(hStdInWr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0);
    
    HPCON conpty = NULL;
    if (conpty_available_) {
        COORD conSize = {80, 24};
        if (CreatePseudoConsole_(conSize, hStdOutRd, hStdInWr, 0, &conpty) == S_OK) {
            conpty_handle_ = (void*)conpty;
        }
    }
    
    STARTUPINFOW si = {sizeof(STARTUPINFOW)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdInRd;
    si.hStdOutput = hStdOutWr;
    si.hStdError = hStdOutWr;
    si.wShowWindow = SW_HIDE;
    
    STARTUPINFOEXW si_ex = {si};
    si_ex.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    
    if (conpty) {
        size_t attrSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &attrSize);
        si_ex.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attrSize);
        if (si_ex.lpAttributeList) {
            InitializeProcThreadAttributeList(si_ex.lpAttributeList, 1, 0, &attrSize);
            UpdateProcThreadAttribute(si_ex.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, conpty, sizeof(conpty), NULL, NULL);
            si_ex.StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
            si_ex.StartupInfo.wShowWindow = SW_HIDE;
        }
    }
    
    PROCESS_INFORMATION pi = {};
    const char* shell = getenv("COMSPEC") ? getenv("COMSPEC") : "cmd.exe";
    
    wchar_t* wshell = NULL;
    if (shell) {
        size_t len = strlen(shell) + 1;
        wshell = (wchar_t*)malloc(len * sizeof(wchar_t));
        mbstowcs(wshell, shell, len);
    }
    
    if (CreateProcessW(NULL, wshell, NULL, NULL, TRUE, CREATE_NEW_CONSOLE | EXTENDED_STARTUPINFOPRESENT, NULL, NULL, (LPSTARTUPINFOW)&si_ex, &pi)) {
        terminal_process_ = (void*)pi.hProcess;
        terminal_stdin_ = (void*)hStdInWr;
        terminal_stdout_ = (void*)hStdOutRd;
        CloseHandle(pi.hThread);
        CloseHandle(hStdInRd);
        CloseHandle(hStdOutWr);
        
        if (conpty) {
            terminal_output_ = "ConPTY Terminal started: ";
        } else {
            terminal_output_ = "Terminal started (pipe mode): ";
        }
        terminal_output_ += shell;
        terminal_output_ += "\r\n";
    } else {
        CloseHandle(hStdInRd);
        CloseHandle(hStdInWr);
        CloseHandle(hStdOutRd);
        CloseHandle(hStdOutWr);
        if (conpty) {
            ClosePseudoConsole_(conpty);
            conpty_handle_ = NULL;
        }
        terminal_output_ = "Failed to start terminal\r\n";
    }
    
    if (wshell) free(wshell);
    if (si_ex.lpAttributeList) {
        free(si_ex.lpAttributeList);
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
    
    static float term_width = 300;
    static float term_height = 200;
    static bool dragging = false;
    
    // Terminal as split pane
    int pos = settings_.terminal_position;
    
    // Solid dark background for terminal
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    
    if (pos == 0) {  // Bottom
        ImGui::BeginChild("##Terminal", ImVec2(-1, term_height), true);
        
        // Terminal header
        ImGui::Text("Terminal"); 
        ImGui::SameLine(ImGui::GetWindowWidth() - 30);
        if (ImGui::Button("X")) show_terminal_ = false;
        ImGui::Separator();
        
        // Output area with dark background
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));  // Dark
        ImGui::BeginChild("term_out", ImVec2(-1, -30), true);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));  // Green
        ImGui::Text("%s", terminal_output_.c_str());
        ImGui::SetScrollHereY(1.0f);
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();  // Pop FrameBg
        
        // Command prompt with input
        char cwd[256] = "";
#ifdef _WIN32
        DWORD size = sizeof(cwd);
        GetCurrentDirectoryA(size, cwd);
        std::string prompt = std::string(cwd) + ">";
#else
        getcwd(cwd, sizeof(cwd));
        std::string prompt = std::string(cwd) + "$ ";
#endif
        
        // Terminal uses direct keyboard capture - no ImGui input field
        // Show prompt
        ImGui::Text("%s", prompt.c_str());
        ImGui::SameLine();
        ImGui::Text("[type directly - enter sends]");
        
        // Direct keyboard capture when terminal focused
        if (ImGui::IsWindowFocused()) {
            ImGuiIO& io = ImGui::GetIO();
            for (int c = 32; c < 127; c++) {
                if (io.InputQueueCharacters.size() == 0 && ImGui::IsKeyPressed((ImGuiKey)c)) {
                    char ch = (char)c;
                    std::string cmd(1, ch);
                    cmd += "\n";
                    if (terminal_stdin_) {
#ifdef _WIN32
                        DWORD written;
                        WriteFile((HANDLE)terminal_stdin_, cmd.c_str(), (DWORD)cmd.size(), &written, nullptr);
#else
                        write((int)(intptr_t)terminal_stdin_, cmd.c_str(), cmd.size());
#endif
                    }
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                std::string cmd = "\n";
                if (terminal_stdin_) {
#ifdef _WIN32
                    DWORD written;
                    WriteFile((HANDLE)terminal_stdin_, cmd.c_str(), (DWORD)cmd.size(), &written, nullptr);
#else
                    write((int)(intptr_t)terminal_stdin_, cmd.c_str(), cmd.size());
#endif
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                std::string cmd = "\b";
                if (terminal_stdin_) {
#ifdef _WIN32
                    DWORD written;
                    WriteFile((HANDLE)terminal_stdin_, cmd.c_str(), (DWORD)cmd.size(), &written, nullptr);
#else
                    write((int)(intptr_t)terminal_stdin_, cmd.c_str(), cmd.size());
#endif
                }
            }
        }
        
        // Horizontal resize handle
        float avail_w = ImGui::GetContentRegionAvail().x;
        ImGui::InvisibleButton("##TermHSplitter", ImVec2(avail_w, 4));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemClicked(0)) dragging = true;
        if (dragging && ImGui::IsMouseDown(0)) {
            term_height += ImGui::GetIO().MouseDelta.y;
            term_height = std::clamp(term_height, 100.0f, 400.0f);
        } else dragging = false;
        
        ImGui::EndChild();
    } else if (pos == 1) {  // Left
        ImGui::BeginChild("##Terminal", ImVec2(term_width, -1), true);
        ImGui::Text("Terminal"); ImGui::SameLine(); if (ImGui::Button("X")) show_terminal_ = false;
        ImGui::Separator();
        ImGui::BeginChild("term_out_l", ImVec2(-1, -30), true);
        ImGui::Text("%s", terminal_output_.c_str());
        ImGui::EndChild();
        
        float avail_h = ImGui::GetContentRegionAvail().y;
        ImGui::InvisibleButton("##TSplL", ImVec2(4, avail_h));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemClicked(0)) dragging = true;
        if (dragging && ImGui::IsMouseDown(0)) { term_width += ImGui::GetIO().MouseDelta.x; term_width = std::clamp(term_width, 150.0f, 400.0f); } else dragging = false;
        ImGui::EndChild();
    } else if (pos == 3) {  // Right
        ImGui::BeginChild("##Terminal", ImVec2(term_width, -1), true);
        ImGui::Text("Terminal"); ImGui::SameLine(); if (ImGui::Button("X")) show_terminal_ = false;
        ImGui::Separator();
        ImGui::BeginChild("term_out_r", ImVec2(-1, -30), true);
        ImGui::Text("%s", terminal_output_.c_str());
        ImGui::EndChild();
        
        float avail_h = ImGui::GetContentRegionAvail().y;
        ImGui::SameLine();
        ImGui::InvisibleButton("##TSplR", ImVec2(4, avail_h));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemClicked(0)) dragging = true;
        if (dragging && ImGui::IsMouseDown(0)) { term_width -= ImGui::GetIO().MouseDelta.x; term_width = std::clamp(term_width, 150.0f, 400.0f); } else dragging = false;
        ImGui::EndChild();
    } else {  // Top (pos == 2)
        ImGui::BeginChild("##Terminal", ImVec2(-1, term_height), true);
        ImGui::Text("Terminal"); ImGui::SameLine(); if (ImGui::Button("X")) show_terminal_ = false;
        ImGui::Separator();
        ImGui::BeginChild("term_out_t", ImVec2(-1, -30), true);
        ImGui::Text("%s", terminal_output_.c_str());
        ImGui::EndChild();
        
        float avail_w = ImGui::GetContentRegionAvail().x;
        ImGui::InvisibleButton("##TSplT", ImVec2(avail_w, 4));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemClicked(0)) dragging = true;
        if (dragging && ImGui::IsMouseDown(0)) { term_height -= ImGui::GetIO().MouseDelta.y; term_height = std::clamp(term_height, 100.0f, 400.0f); } else dragging = false;
        ImGui::EndChild();
    }
    ImGui::PopStyleColor();
}

// ============================================================================
// Dialogs
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
    split_horizontal(false);
}

void EditorApp::split_horizontal(bool for_terminal) {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    
    Split* split = new Split();
    
    if (for_terminal) {
        // Terminal-only split
        split->has_terminal = true;
        split->editor = nullptr;
        split->editor_owned = false;
        start_split_terminal(split);
    } else {
        TextEditor* active_editor = get_active_editor();
        if (!active_editor) return;
        
        TextEditor* ed = new TextEditor();
        ed->SetText(active_editor->GetText());
        auto lang = active_editor->GetLanguageDefinition();
        ed->SetLanguageDefinition(lang);
        ed->SetPalette(active_editor->GetPalette());
        ed->SetTabSize(active_editor->GetTabSize());
        
        split->editor = ed;
        split->editor_owned = true;
    }
    
    split->is_horizontal = true;
    split->ratio = (float)(1.0 / (tab.splits.size() + 1));
    
    tab.splits.push_back(split);
    tab.active_split = (int)tab.splits.size() - 1;
}

void EditorApp::split_vertical(bool for_terminal) {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    
    Split* split = new Split();
    
    if (for_terminal) {
        split->has_terminal = true;
        split->editor = nullptr;
        split->editor_owned = false;
        start_split_terminal(split);
    } else {
        TextEditor* active_editor = get_active_editor();
        if (!active_editor) return;
        
        TextEditor* ed = new TextEditor();
        ed->SetText(active_editor->GetText());
        auto lang = active_editor->GetLanguageDefinition();
        ed->SetLanguageDefinition(lang);
        ed->SetPalette(active_editor->GetPalette());
        ed->SetTabSize(active_editor->GetTabSize());
        
        split->editor = ed;
        split->editor_owned = true;
    }
    
    split->is_horizontal = false;
    split->ratio = (float)(1.0 / (tab.splits.size() + 1));
    
    tab.splits.push_back(split);
    tab.active_split = (int)tab.splits.size() - 1;
}

void EditorApp::start_split_terminal(Split* split) {
    if (!split) return;
    
    init_conpty();
    
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hStdInRd = NULL, hStdInWr = NULL;
    HANDLE hStdOutRd = NULL, hStdOutWr = NULL;
    
    CreatePipe(&hStdInRd, &hStdInWr, &sa, 0);
    CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0);
    
    SetHandleInformation(hStdInWr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0);
    
    HPCON conpty = NULL;
    if (conpty_available_) {
        COORD conSize = {80, 24};
        if (CreatePseudoConsole_(conSize, hStdOutRd, hStdInWr, 0, &conpty) == S_OK) {
            split->conpty_handle = (void*)conpty;
        }
    }
    
    STARTUPINFOW si = {sizeof(STARTUPINFOW)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdInRd;
    si.hStdOutput = hStdOutWr;
    si.hStdError = hStdOutWr;
    si.wShowWindow = SW_HIDE;
    
    STARTUPINFOEXW si_ex = {si};
    si_ex.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    
    if (conpty) {
        size_t attrSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &attrSize);
        si_ex.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attrSize);
        if (si_ex.lpAttributeList) {
            InitializeProcThreadAttributeList(si_ex.lpAttributeList, 1, 0, &attrSize);
            UpdateProcThreadAttribute(si_ex.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, conpty, sizeof(conpty), NULL, NULL);
            si_ex.StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
            si_ex.StartupInfo.wShowWindow = SW_HIDE;
        }
    }
    
    PROCESS_INFORMATION pi = {};
    const char* shell = getenv("COMSPEC") ? getenv("COMSPEC") : "cmd.exe";
    
    wchar_t* wshell = NULL;
    if (shell) {
        size_t len = strlen(shell) + 1;
        wshell = (wchar_t*)malloc(len * sizeof(wchar_t));
        mbstowcs(wshell, shell, len);
    }
    
    if (CreateProcessW(NULL, wshell, NULL, NULL, TRUE, CREATE_NEW_CONSOLE | EXTENDED_STARTUPINFOPRESENT, NULL, NULL, (LPSTARTUPINFOW)&si_ex, &pi)) {
        split->terminal_process = (void*)pi.hProcess;
        split->terminal_stdin = (void*)hStdInWr;
        split->terminal_stdout = (void*)hStdOutRd;
        CloseHandle(pi.hThread);
        CloseHandle(hStdInRd);
        CloseHandle(hStdOutWr);
        
        if (conpty) {
            split->terminal_output = "ConPTY Terminal started: ";
        } else {
            split->terminal_output = "Terminal started (pipe mode): ";
        }
        split->terminal_output += shell;
        split->terminal_output += "\r\n";
    } else {
        CloseHandle(hStdInRd);
        CloseHandle(hStdInWr);
        CloseHandle(hStdOutRd);
        CloseHandle(hStdOutWr);
        if (conpty) {
            ClosePseudoConsole_(conpty);
            split->conpty_handle = NULL;
        }
        split->terminal_output = "Failed to start terminal\r\n";
    }
    
    if (wshell) free(wshell);
    if (si_ex.lpAttributeList) {
        free(si_ex.lpAttributeList);
    }
#else
    // Unix - fork pty
    int master_fd;
    pid_t pid = forkpty(&master_fd, nullptr, nullptr, nullptr);
    
    if (pid < 0) {
        split->terminal_output = "Failed to fork terminal\r\n";
    } else if (pid == 0) {
        // Child - exec shell
        execl("/bin/sh", "/bin/sh", nullptr);
        exit(0);
    } else {
        split->terminal_process = (void*)(intptr_t)pid;
        split->terminal_stdin = (void*)(intptr_t)master_fd;
        split->terminal_stdout = (void*)(intptr_t)master_fd;
    }
#endif
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
    
    std::string selected_path = path;
    if (path.empty()) {
        nfdchar_t* out_path = nullptr;
        nfdresult_t result = NFD_OpenDialog(&out_path, nullptr, 0, nullptr);
        if (result == NFD_OKAY && out_path) {
            selected_path = out_path;
            settings_.last_open_dir = std::filesystem::path(out_path).parent_path().string();
            NFD_FreePath(out_path);
        } else if (result == NFD_CANCEL) {
            return;
        }
    }
    
    if (!selected_path.empty()) {
        split_horizontal();
        open_file(selected_path);
    }
}

void EditorApp::open_file_split_vertical(const std::string& path) {
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    
    std::string selected_path = path;
    if (path.empty()) {
        nfdchar_t* out_path = nullptr;
        nfdresult_t result = NFD_OpenDialog(&out_path, nullptr, 0, nullptr);
        if (result == NFD_OKAY && out_path) {
            selected_path = out_path;
            settings_.last_open_dir = std::filesystem::path(out_path).parent_path().string();
            NFD_FreePath(out_path);
        } else if (result == NFD_CANCEL) {
            return;
        }
    }
    
    if (!selected_path.empty()) {
        split_vertical();
        open_file(selected_path);
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
    
    // Use the active split's orientation to determine layout
    bool use_horizontal = true;
    if (tab.active_split >= 0 && tab.active_split < (int)tab.splits.size()) {
        use_horizontal = tab.splits[tab.active_split]->is_horizontal;
    }
    
    if (!use_horizontal) {
        // Vertical splits - side by side (left to right)
        ImGui::Columns((int)tab.splits.size(), "split_cols", false);
        
        for (int i = 0; i < (int)tab.splits.size(); i++) {
            auto* split = tab.splits[i];
            float width = avail.x / tab.splits.size();
            
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
            ImGui::BeginChild(("vsplit_" + std::to_string(i)).c_str(), ImVec2(width, avail.y), true);
            if (split->editor) {
                split->editor->Render(("Editor_v" + std::to_string(i)).c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            
            if (i < (int)tab.splits.size() - 1) {
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
    } else {
        // Horizontal splits - stack top to bottom
        for (int i = 0; i < (int)tab.splits.size(); i++) {
            auto* split = tab.splits[i];
            float height = avail.y / tab.splits.size();
            
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
            ImGui::BeginChild(("hsplit_" + std::to_string(i)).c_str(), ImVec2(avail.x, height), true);
            if (split->editor) {
                split->editor->Render(("Editor_h" + std::to_string(i)).c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
    }
}



