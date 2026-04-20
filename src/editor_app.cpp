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
#include "imgui_notify.h"
#include "editor_notifications.h"
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
#include <functional>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace EditorNotifications;

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
    
    f << "  \"enable_vim_mode\": " << (s.enable_vim_mode ? "true" : "false") << ",\n";
    f << "  \"word_wrap\": " << (s.word_wrap ? "true" : "false") << ",\n";
    f << "  \"show_line_numbers\": " << (s.show_line_numbers ? "true" : "false") << ",\n";
    f << "  \"show_minimap\": " << (s.show_minimap ? "true" : "false") << ",\n";
    f << "  \"show_spaces\": " << (s.show_spaces ? "true" : "false") << ",\n";
    f << "  \"highlight_line\": " << s.highlight_line << ",\n";
    f << "  \"show_tabs\": " << (s.show_tabs ? "true" : "false") << ",\n";
    f << "  \"notifications_enabled\": " << (s.notifications_enabled ? "true" : "false") << ",\n";
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
    
    s.enable_vim_mode = false;  // Force disabled - user can't edit with vim on
    s.word_wrap = get_bool("word_wrap", false);
    s.show_line_numbers = get_bool("show_line_numbers", true);
    s.show_bookmark_margin = get_bool("show_bookmark_margin", true);
    s.show_change_history = get_bool("show_change_history", true);
    s.show_minimap = get_bool("show_minimap", true);
    s.show_spaces = get_bool("show_spaces", false);
    s.highlight_line = get_int("highlight_line", 1);
    s.show_tabs = get_bool("show_tabs", false);
    s.notifications_enabled = get_bool("notifications_enabled", true);
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
// Version: 0.9.11
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
        version = "0.8.0 (442ca2885e44d8cf8ee8f2a9a6ddb5eb6abf2de6)";
    }
    return "pCode Editor " + version;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
EditorApp::EditorApp(int argc, char* argv[])
    : argc_(argc), argv_(argv), notification_manager_(&NotificationManager::get()) {
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
        ImGuiIO& io = ImGui::GetIO();
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
            for (int i = 0; i < count; i++) {
                app->open_file(paths[i]);
            }
        }
    });
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
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
    // Leverage default ImGui styles
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Base styling
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.PopupRounding = 4.0f;
    
    // Colors - use ImGui defaults, enhance for editor
    ImVec4* colors = style.Colors;
    // Dark Pro Palette
    // Text high contrast: #E6EEF6
    colors[ImGuiCol_Text] = dark ? ImVec4(0.902f, 0.933f, 0.965f, 1.00f) : ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    // Text muted: #9AA6B2
    colors[ImGuiCol_TextDisabled] = dark ? ImVec4(0.604f, 0.651f, 0.698f, 1.00f) : ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    // Background: #0B1020
    colors[ImGuiCol_WindowBg] = dark ? ImVec4(0.043f, 0.063f, 0.125f, 1.00f) : ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_ChildBg] = dark ? ImVec4(0.067f, 0.078f, 0.125f, 1.00f) : ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    // Surface: #111420 for popups
    colors[ImGuiCol_PopupBg] = dark ? ImVec4(0.067f, 0.078f, 0.125f, 1.00f) : ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    // Border: #1F2632
    colors[ImGuiCol_Border] = dark ? ImVec4(0.122f, 0.149f, 0.196f, 1.00f) : ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_BorderShadow] = dark ? ImVec4(0.00f, 0.00f, 0.00f, 0.00f) : ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    // Surface for frames
    colors[ImGuiCol_FrameBg] = dark ? ImVec4(0.067f, 0.078f, 0.125f, 1.00f) : ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = dark ? ImVec4(0.361f, 0.882f, 0.902f, 0.30f) : ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = dark ? ImVec4(0.361f, 0.882f, 0.902f, 0.50f) : ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_TitleBg] = dark ? ImVec4(0.067f, 0.078f, 0.125f, 1.00f) : ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = dark ? ImVec4(0.067f, 0.078f, 0.125f, 1.00f) : ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = dark ? ImVec4(0.067f, 0.078f, 0.125f, 1.00f) : ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    
    // Additional styles
    colors[ImGuiCol_MenuBarBg] = dark ? ImVec4(0.067f, 0.078f, 0.125f, 1.00f) : ImVec4(0.93f, 0.93f, 0.93f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = dark ? ImVec4(0.067f, 0.078f, 0.125f, 1.00f) : ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = dark ? ImVec4(0.478f, 0.424f, 1.0f, 0.50f) : ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = dark ? ImVec4(0.478f, 0.424f, 1.0f, 0.70f) : ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = dark ? ImVec4(0.478f, 0.424f, 1.0f, 1.00f) : ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    // Button with accent
    colors[ImGuiCol_Button] = dark ? ImVec4(0.361f, 0.882f, 0.902f, 0.30f) : ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = dark ? ImVec4(0.361f, 0.882f, 0.902f, 0.50f) : ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_ButtonActive] = dark ? ImVec4(0.361f, 0.882f, 0.902f, 0.70f) : ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    // Separator: #1F2632
    colors[ImGuiCol_Separator] = dark ? ImVec4(0.122f, 0.149f, 0.196f, 1.00f) : ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    // Tab: surface
    colors[ImGuiCol_Tab] = dark ? ImVec4(0.067f, 0.078f, 0.125f, 1.00f) : ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TabHovered] = dark ? ImVec4(0.122f, 0.149f, 0.196f, 1.00f) : ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_TabActive] = dark ? ImVec4(0.122f, 0.149f, 0.196f, 1.00f) : ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    // Menu highlight - matches editor selection
    colors[ImGuiCol_Header] = dark ? ImVec4(0.149f, 0.149f, 0.149f, 1.00f) : ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = dark ? ImVec4(0.25f, 0.25f, 0.25f, 1.00f) : ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_HeaderActive] = dark ? ImVec4(0.20f, 0.20f, 0.20f, 1.00f) : ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    
    // Sync editor background to theme
    for (auto& tab : tabs_) {
        if (tab.editor) {
            auto palette = dark ? TextEditor::GetDarkPalette() : TextEditor::GetLightPalette();
            palette[(int)TextEditor::PaletteIndex::Background] = dark ? 0xFF20100B : 0xFFF6F6F6;
            tab.editor->SetPalette(palette);
            for (auto* split : tab.splits) {
                if (split->editor) {
                    split->editor->SetPalette(palette);
                }
            }
        }
    }
}

// ============================================================================
// Tab management
// ============================================================================
static int untitled_count = 0;

void EditorApp::new_tab() {
    EditorTab tab;
    untitled_count++;
    tab.display_name = "untitled" + std::to_string(untitled_count);
    tab.editor = new TextEditor();
    tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    tab.editor->SetTabSize(settings_.tab_size);
    // Set editor background to match theme scrollbar background
    auto palette = settings_.dark_theme ? TextEditor::GetDarkPalette() : TextEditor::GetLightPalette();
    palette[(int)TextEditor::PaletteIndex::Background] = settings_.dark_theme ? 0xFF20100B : 0xFFF6F6F6;
    tab.editor->SetPalette(palette);

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
        nfdresult_t result = NFD_OpenDialog(&out_path, nullptr, 0, nullptr);

        if (result == NFD_OKAY && out_path) {
            selected_path = out_path;
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
        ImGui::AddErrorNotification("Failed to save file");
        return false;
    }
    file << editor->GetText();
    file.close();
    tab.dirty = false;
    tab.file_size = editor->GetText().size();
    tab.line_ending = get_line_ending(editor->GetText());
    add_recent_file(tab.file_path);
    ImGui::AddSuccessNotification("File saved successfully");
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
        // Show confirmation dialog
        pending_close_tab_idx_ = idx;
        ImGui::OpenPopup("Discard Changes?");
    } else {
        delete tabs_[idx].editor;
        tabs_.erase(tabs_.begin() + idx);
        if (active_tab_ >= (int)tabs_.size()) {
            active_tab_ = (int)tabs_.size() - 1;
        }
        if (active_tab_ > idx) active_tab_--;
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
    // status bar removed
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
    
    // Apply theme to all tabs + splits
    uint32_t bg = settings_.dark_theme ? 0xFF20100B : 0xFFF6F6F6;
    for (auto& tab : tabs_) {
        auto palette = settings_.dark_theme ? TextEditor::GetDarkPalette() : TextEditor::GetLightPalette();
        palette[(int)TextEditor::PaletteIndex::Background] = bg;
        tab.editor->SetPalette(palette);
        for (auto* split : tab.splits) {
            if (split->editor) split->editor->SetPalette(palette);
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
    if (true) {
        float status_y = ew_pos.y + ew_h - 24;
        ImVec2 status_pos = ImGui::GetWindowPos(); // Current window context
        // Check if status bar renders at bottom inside Editor
        ImGui::SetCursorPos(ImVec2(0, ew_h - 24 - ImGui::GetStyle().ScrollbarSize));
        ImVec2 cursor = ImGui::GetCursorPos();
        bool status_ok = cursor.y > 0 && cursor.y < ew_h;
        printf("[STATUS] %s (bottom, inside editor) - %s\n", 
            status_ok ? "OK" : "FAIL",
            true ? "enabled" : "disabled");
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
    
    if (true && settings_.show_minimap) {
        printf("NOTE: Enable components via View menu, then check:\n");
        printf("  - Status bar should be at bottom of Editor\n");
        printf("  - Minimap should be at right side\n");
        printf("  - Gutter should be at left of code\n");
    }
    
    bool has_status = false;
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
    
    if (io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_O)) { open_file(""); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_N)) { new_tab(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_W)) { close_tab(active_tab_); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) { toggle_explorer(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_F)) { show_find_ = true; return; }
        if (ImGui::IsKeyPressed(ImGuiKey_H)) { show_replace_ = true; return; }
        if (ImGui::IsKeyPressed(ImGuiKey_G)) { show_goto_ = true; return; }
        if (ImGui::IsKeyPressed(ImGuiKey_A)) { 
            if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
                get_active_editor()->SelectAll(); 
            }
            return; 
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) { 
            if (tabs_.size() > 1) {
                active_tab_ = (active_tab_ + 1) % tabs_.size();
                ImGui::SetWindowFocus("Editor");
            }
            return; 
        }
    }
    if (io.KeyCtrl && io.KeyShift && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_F12)) { show_style_editor_ = true; return; }
        if (ImGui::IsKeyPressed(ImGuiKey_N)) { new_window(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_S)) { save_tab_as(active_tab_); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_P)) { show_cmd_palette_ = true; return; }
        if (ImGui::IsKeyPressed(ImGuiKey_H)) { split_horizontal(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_V)) { split_vertical(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_Tab)) { 
            if (tabs_.size() > 1) {
                active_tab_ = (active_tab_ > 0) ? active_tab_ - 1 : (int)tabs_.size() - 1;
                ImGui::SetWindowFocus("Editor");
            }
            return; 
        }
    }
    if (io.KeyCtrl && io.KeyAlt && !io.KeyShift) {
        if (ImGui::IsKeyPressed(ImGuiKey_S)) { save_all(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_W)) { close_window(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_H)) { prev_split(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_L)) { next_split(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_K)) { rotate_splits(); return; }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) { equalize_splits(); return; }
    }
    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (show_find_) { show_find_ = false; return; }
            if (show_replace_) { show_replace_ = false; return; }
            if (show_goto_) { show_goto_ = false; return; }
        }
    }
    if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
        // Removed spacebar command palette - was blocking normal typing
    }
    
    // Clear InputQueueCharacters ONLY when vim mode is enabled and in Normal mode
    if (settings_.enable_vim_mode && vim_mode_ == VimMode::Normal && !terminal_input_active_) {
        ImGuiIO& io = ImGui::GetIO();
        io.InputQueueCharacters.resize(0);
    }
    
    // Command mode - don't process vim keys or shortcuts, let status bar handle input
    if (vim_mode_ != VimMode::Command) {
        if (io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_Space)) { show_cmd_palette_ = true; return; }
            if (ImGui::IsKeyPressed(ImGuiKey_O)) { open_file(""); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_S)) { save_tab(active_tab_); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_N)) { new_tab(); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_W)) { close_tab(active_tab_); return; }
            if (ImGui::IsKeyPressed(ImGuiKey_E)) { toggle_explorer(); return; }
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
            if (ImGui::IsKeyPressed(ImGuiKey_F12)) { show_style_editor_ = true; return; }
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
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(vp->Size, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::Begin("pCode Editor", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);
    // NOTE: SetWindowFocus removed - it was closing menus instantly
    // Always render menus - no conditional
    render_menu_bar();
    
    // Context menu - right click only
    if (ImGui::BeginPopupContextWindow("##ContextMenu")) {
        if (ImGui::MenuItem("New File", "Ctrl+N")) new_tab();
        if (ImGui::MenuItem("Open...", "Ctrl+O")) open_file("");
        ImGui::Separator();
        
        ImGui::Separator();
        if (ImGui::MenuItem("Git", nullptr, &show_git_changes_)) {}
        if (ImGui::MenuItem("Symbol", nullptr, &show_symbol_outline_)) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Line Numbers", nullptr, &settings_.show_line_numbers)) toggle_line_numbers();
        ImGui::EndPopup();
    }
    
    render_editor_area();
    ImGui::End();

    // Dialogs
    if (show_find_) render_find_dialog();
    if (show_replace_) render_replace_dialog();
    if (show_goto_) render_goto_dialog();
    if (show_font_) render_font_dialog();
    if (show_spaces_dialog_) render_spaces_dialog();
    if (show_cmd_palette_) render_command_palette();
    if (show_about_) {
        // Guard: ensure about dialog doesn't hang
        if (!ImGui::IsPopupOpen("About")) {
            ImGui::OpenPopup("About");
        }
        render_about_dialog();
    }
    if (show_style_editor_) {
        // Guard: prevent hanging style editor
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Once);
        if (ImGui::Begin("Style Editor", &show_style_editor_)) {
            ImGui::ShowStyleEditor(nullptr);
            ImGui::End();
        }
    }
    
    // Native Windows status bar
    update_native_status_bar();
    
    // Render notifications (toasts)
    ImGui::RenderNotifications();
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
        render_menu_options();
        render_menu_help();
        ImGui::EndMenuBar();
    }
}

void EditorApp::render_menu_file() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Tab", "Ctrl+N")) new_tab();
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
        ImGui::EndMenu();
    }
}

void EditorApp::render_menu_view() {
    if (ImGui::BeginMenu("View")) {
        // Vim mode - removed
        if (ImGui::BeginMenu("Zoom")) {
            if (ImGui::MenuItem("Zoom In", "Ctrl++")) zoom_in();
            if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) zoom_out();
            if (ImGui::MenuItem("Restore Default Zoom", "Ctrl+0")) zoom_reset();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        
        // Show Tabs (default: false)
        bool tabs = settings_.show_tabs;
        if (ImGui::MenuItem("Show Tabs", nullptr, &tabs)) settings_.show_tabs = tabs;
        
        // Explorer (default: true)
        bool explorer = (explorer_side_ != -1);
        if (ImGui::MenuItem("Explorer", "Ctrl+E", &explorer)) {
            if (!explorer) explorer_side_ = -1;
            else if (explorer_side_ == -1) explorer_side_ = 0;
            else explorer_side_ = (explorer_side_ == 0) ? 1 : 0;
            show_file_tree_ = (explorer_side_ != -1);
        }
        
        ImGui::Separator();
        if (ImGui::MenuItem("Validate Layout", "Ctrl+Shift+L")) validate_layout();
        
        ImGui::Separator();
        if (ImGui::BeginMenu("Spaces")) {
            if (ImGui::MenuItem("2 Spaces", nullptr, settings_.tab_size == 2)) set_tab_size(2);
            if (ImGui::MenuItem("4 Spaces", nullptr, settings_.tab_size == 4)) set_tab_size(4);
            if (ImGui::MenuItem("8 Spaces", nullptr, settings_.tab_size == 8)) set_tab_size(8);
if (ImGui::MenuItem("Custom...")) { show_spaces_dialog_ = true; tab_size_temp_ = settings_.tab_size; }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(settings_.dark_theme ? "Light Theme" : "Dark Theme")) toggle_theme();
        ImGui::EndMenu();
    }
}

void EditorApp::render_menu_help() {
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Style Editor", "Ctrl+Shift+F12")) {
            show_style_editor_ = true;
        }
        if (ImGui::MenuItem("Test Notifications")) {
            test_notifications();
        }
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

void EditorApp::render_menu_options() {
    if (ImGui::BeginMenu("Options")) {
        bool notif = settings_.notifications_enabled;
        if (ImGui::MenuItem("Notifications", nullptr, &notif)) {
            settings_.notifications_enabled = notif;
        }
        if (ImGui::MenuItem("Font")) {
            show_font_ = true;
        }
        if (ImGui::MenuItem("Spaces")) {
            show_spaces_dialog_ = true;
        }
        ImGui::EndMenu();
    }
}

void EditorApp::render_about_dialog() {
    if (!show_about_) return;
    
    // Guard: prevent transparent/hanging dialog - only open once
    if (!ImGui::IsPopupOpen("About")) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(440, 420), ImGuiCond_Always);
        ImGui::OpenPopup("About");
    }
    
    // Push opaque background color (not transparent)
    ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    bg.w = 1.0f;  // Force full alpha
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
    
    if (ImGui::BeginPopupModal("About", &show_about_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Dummy(ImVec2(4, 0));  // Left margin
        
        ImGui::Text("Personal Code Editor");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));
        
        // VERSION format: "0.9.10 (hash)" - show short version
        std::string version = get_version();
        size_t paren = version.find(" (");
        std::string short_ver = version.substr(0, paren);
        if (paren != std::string::npos) {
            std::string hash = version.substr(paren + 2);
            size_t close_paren = hash.find(")");
            if (close_paren != std::string::npos) {
                short_ver += " (" + hash.substr(0, close_paren) + ")";
            }
        }
        ImGui::Text("Version: %s", short_ver.c_str());
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
        ImGui::PopStyleColor();
    }
}

// ============================================================================
// Notification Testing
// ============================================================================
void EditorApp::test_notifications() {
    if (!settings_.notifications_enabled) return;
    
    // Trigger a variety of sample notifications to test the system
    
    // Build failure
    notify_build_failure(
        "MyProject", "all", "src/main.cpp", 42,
        "undefined reference to 'foo'", "build#123", "logs/build.log"
    );
    
    // Tests failing
    notify_tests_failure(
        "unit-tests", 3, "TestFoo", "expected equal", "tests/foo"
    );
    
    // CI status change
    notify_ci_status(
        "PR-42", "pending", "failed", "compile",
        "github-actions", "https://ci.example.com/pr/42"
    );
    
    // Review request (draft)
    notify_review_request(
        "Add feature X", "alice", 5,
        {"needs-review", "WIP"}, "123", "https://pr.example.com/123", true
    );
    
    // Security alert
    notify_security_alert(
        "src/crypto.cpp", "HIGH", "1.2.3", "1.2.4",
        "https://security.example.com/advisory/456"
    );
    
    // Dependency vulnerability
    notify_dependency_vuln(
        "openssl", "CRITICAL", "<1.1.1t", "1.1.1t",
        "CVE-2024-1234", "https://nvd.nist.gov/vuln/detail/CVE-2024-1234"
    );
    
    // Task complete (success)
    notify_task_complete(
        "code generation", true, 45, "artifacts/gen.tar.gz", ""
    );
    
    // Git event (force-push)
    notify_git_event(
        "main", "force-push", "src/", 0, "bob", ""
    );
    
    // Performance regression
    notify_perf_regression(
        "startup_time", -15.0f, 100.0f, 85.0f, "initialization", ""
    );
}

// ============================================================================
// Editor Area
// ============================================================================
void EditorApp::render_editor_area() {
    // Terminal disabled - remove entirely
    // if (show_terminal_) { render_terminal(); }
    
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

    float status_height = true ? 24 : 0;
    // Use content region directly - this accounts for everything
    float editor_width = ImGui::GetContentRegionAvail().x;
    float editor_height = ImGui::GetContentRegionAvail().y;
    
    if (tabs_.empty()) {
        new_tab();
    }

    bool show_tabs = settings_.show_tabs;
    float tab_height = show_tabs ? ImGui::GetFrameHeight() : 0.0f;
    float status_h = true ? 24.0f : 0.0f;
    float editor_area_width = editor_width;
    float editor_area_height = editor_height - tab_height - status_h;
    static int prev_active_tab = -1;
    
    if (prev_active_tab != active_tab_ && prev_active_tab >= 0 && prev_active_tab < (int)tabs_.size()) {
        auto& prev_tab = tabs_[prev_active_tab];
        prev_tab.editor->GetCursorPosition();
    }
prev_active_tab = active_tab_;
    
// ImGui tabs with close buttons
    if (show_tabs) {
        if (ImGui::BeginTabBar("##Tabs")) {
            for (int i = 0; i < (int)tabs_.size(); i++) {
                auto& tab = tabs_[i];
                bool open = true;
                if (ImGui::BeginTabItem(tab.display_name.c_str(), &open, ImGuiTabItemFlags_None)) {
                    active_tab_ = i;
                    ImGui::EndTabItem();
                }
                if (!open) {
                    close_tab(i);
                    break;
                }
            }
            ImGui::EndTabBar();
        }
        // [+] for new tab
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            new_tab();
        }
    }
    
// Editor
    if (active_tab_ >= 0 && active_tab_ < (int)tabs_.size()) {
        render_editor_with_margins();
    }
    
    // Simple file explorer
    if (show_file_tree_) {
        ImGui::SameLine();
        if (ImGui::BeginChild("##Explorer", ImVec2(200, 0), true)) {
            static std::vector<std::string> expanded_dirs;
            std::string dir = settings_.last_open_dir.empty() ? "." : settings_.last_open_dir;
            
            try {
                for (auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string name = entry.path().filename().string();
                    bool is_dir = entry.is_directory();
                    std::string full_path = entry.path().string();
                    
                    // Check if expanded
                    bool expanded = false;
                    for (auto& e : expanded_dirs) {
                        if (e == full_path) { expanded = true; break; }
                    }
                    
                    // Icon: >/v for dirs, · for files
                    std::string icon = is_dir ? (expanded ? "v " : "> ") : "· ";
                    std::string id = icon + name + "##" + full_path;
                    
                    if (ImGui::Selectable(id.c_str(), false)) {
                        if (is_dir) {
                            if (expanded) {
                                expanded_dirs.erase(std::remove(expanded_dirs.begin(), expanded_dirs.end(), full_path));
                            } else {
                                expanded_dirs.push_back(full_path);
                            }
                        } else {
                            open_file(full_path);
                        }
                    }
                }
            } catch (...) {}
            ImGui::EndChild();
        }
    }
    
    // Render discard confirmation modal
    if (ImGui::BeginPopupModal("Discard Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save changes to '%s'?", tabs_[pending_close_tab_idx_].display_name.c_str());
        ImGui::Separator();
        if (ImGui::Button("Save")) {
            save_tab(pending_close_tab_idx_);
            delete tabs_[pending_close_tab_idx_].editor;
            tabs_.erase(tabs_.begin() + pending_close_tab_idx_);
            if (active_tab_ >= (int)tabs_.size()) {
                active_tab_ = (int)tabs_.size() - 1;
            }
            if (active_tab_ > pending_close_tab_idx_) active_tab_--;
            pending_close_tab_idx_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save")) {
            delete tabs_[pending_close_tab_idx_].editor;
            tabs_.erase(tabs_.begin() + pending_close_tab_idx_);
            if (active_tab_ >= (int)tabs_.size()) {
                active_tab_ = (int)tabs_.size() - 1;
            }
            if (active_tab_ > pending_close_tab_idx_) active_tab_--;
            pending_close_tab_idx_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pending_close_tab_idx_ = -1;
            ImGui::CloseCurrentPopup();
        }
ImGui::EndPopup();
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

void EditorApp::start_terminal() {
    // Terminal disabled
}

void EditorApp::render_editor_with_margins() {
    // Simplified - just render active editor
    if (active_tab_ < 0 || active_tab_ >= (int)tabs_.size()) return;
    auto& tab = tabs_[active_tab_];
    if (!tab.splits.empty()) {
        render_splits(active_tab_);
        return;
    }
    TextEditor* editor = get_active_editor();
    if (editor) editor->Render("Editor");
}

void EditorApp::split_horizontal(bool) {
    // Stub - splits disabled
}

void EditorApp::split_vertical(bool) {
    // Stub - splits disabled
}

void EditorApp::split_horizontal() {
    split_horizontal(false);
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
    
    // Clear first_render flag after first render call
    if (tab.first_render) tab.first_render = false;
    
    if (tab.splits.empty()) {
        if (!tab.first_render && tab.editor->IsTextChanged()) tab.dirty = true;
        tab.editor->Render("TextEditor");
        ImGui::GetIO().FontGlobalScale = old_scale;
        return;
    }
    
    if (tab.splits.size() == 1) {
        if (!tab.first_render && tab.splits[0]->editor->IsTextChanged()) tab.dirty = true;
        tab.first_render = false;
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
            // Use stored ratio for first split
            float width = split->ratio * avail.x;
            if (i == 0) width = (1.0f - split->ratio) * avail.x;
            
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
            ImGui::BeginChild(("vsplit_" + std::to_string(i)).c_str(), ImVec2(width, avail.y), true);
            if (split->editor) {
                if (split->editor->IsTextChanged()) tab.dirty = true;
                split->editor->Render(("Editor_v" + std::to_string(i)).c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            
            // Resizer between splits
            if (i < (int)tab.splits.size() - 1) {
                ImGui::NextColumn();
                // Drag to resize
                ImVec2 cursor = ImGui::GetCursorPos();
                ImGui::PushID(i);
                if (ImGui::InvisibleButton("vsplitter", ImVec2(4, -1))) {}
                if (ImGui::IsItemActive()) {
                    ImGuiIO& io = ImGui::GetIO();
                    float mouse_x = ImGui::GetMousePos().x - ImGui::GetWindowPos().x;
                    split->ratio = mouse_x / avail.x;
                    if (split->ratio < 0.1f) split->ratio = 0.1f;
                    if (split->ratio > 0.9f) split->ratio = 0.9f;
                    tab.dirty = true;
                }
                ImGui::PopID();
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
    } else {
        // Horizontal splits - stack top to bottom
        float y = 0;
        for (int i = 0; i < (int)tab.splits.size(); i++) {
            auto* split = tab.splits[i];
            float height = split->ratio * avail.y;
            if (i == 0) height = (1.0f - split->ratio) * avail.y;
            
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
            ImGui::BeginChild(("hsplit_" + std::to_string(i)).c_str(), ImVec2(avail.x, height), true);
            if (split->editor) {
                if (split->editor->IsTextChanged()) tab.dirty = true;
                split->editor->Render(("Editor_h" + std::to_string(i)).c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            
            // Resizer between splits
            if (i < (int)tab.splits.size() - 1) {
                ImGui::PushID(i + 100);
                if (ImGui::InvisibleButton("hsplitter", ImVec2(-1, 4))) {}
                if (ImGui::IsItemActive()) {
                    ImGuiIO& io = ImGui::GetIO();
                    float mouse_y = ImGui::GetMousePos().y - ImGui::GetWindowPos().y;
                    split->ratio = mouse_y / avail.y;
                    if (split->ratio < 0.1f) split->ratio = 0.1f;
                    if (split->ratio > 0.9f) split->ratio = 0.9f;
                    tab.dirty = true;
                }
                ImGui::PopID();
            }
        }
    }
}



