#pragma once

#include <string>
#include <vector>
#include <memory>

struct GLFWwindow;

class EditorApp {
public:
    EditorApp();
    ~EditorApp();

    int run();

private:
    void init();
    void shutdown();
    void render();
    void render_menu_bar();
    void render_editor();
    void render_status_bar();
    void render_command_palette();

    void open_file(const std::string& path);
    void save_file();
    void new_file();

    GLFWwindow* window_ = nullptr;
    bool running_ = true;

    // Editor state
    std::string current_file_;
    bool dirty_ = false;
    bool show_command_palette_ = false;
    char command_buffer_[256] = {0};

    // Text editor (ImGuiColorTextEdit)
    void* text_editor_ = nullptr;  // TextEditor* (opaque to avoid header dependency)
};
