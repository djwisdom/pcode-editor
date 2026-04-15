#include "editor_app.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <TextEditor.h>
#include <nfd.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

EditorApp::EditorApp() {
    current_file_ = "untitled";
    text_editor_ = new TextEditor();
}

EditorApp::~EditorApp() {
    delete static_cast<TextEditor*>(text_editor_);
}

int EditorApp::run() {
    init();

    while (running_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render();

        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }

    shutdown();
    return 0;
}

void EditorApp::init() {
    // GLFW setup
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window_ = glfwCreateWindow(1280, 800, "Code Editor", nullptr, nullptr);
    if (!window_) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        exit(1);
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // vsync

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Set up text editor language (C++ default)
    TextEditor* editor = static_cast<TextEditor*>(text_editor_);
    editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    editor->SetText("// Welcome to Code Editor\n// Open a file with Ctrl+O or File > Open\n\n");
}

void EditorApp::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void EditorApp::render() {
    // Keyboard shortcuts (global, before ImGui consumes input)
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_O)) {
            open_file("");
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_S)) {
            save_file();
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_N)) {
            new_file();
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_P)) {
            show_command_palette_ = !show_command_palette_;
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
            running_ = false;
            return;
        }
    }

    // Full-screen dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    // Force "Editor" window into the central dock node on first launch
    // This ensures it fills the entire viewport instead of floating
    ImGui::DockBuilderDockWindow("Editor", dockspace_id);

    render_menu_bar();
    render_editor();
    render_status_bar();

    if (show_command_palette_) {
        render_command_palette();
    }

    ImGui::End();
}

void EditorApp::render_menu_bar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) new_file();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) open_file("");
            if (ImGui::MenuItem("Save", "Ctrl+S")) save_file();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Ctrl+Q")) running_ = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Find...", "Ctrl+F")) {}
            if (ImGui::MenuItem("Replace...", "Ctrl+H")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Command Palette", "Ctrl+P")) show_command_palette_ = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void EditorApp::render_editor() {
    // No title bar, no borders — let the dockspace handle the chrome
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    TextEditor* editor = static_cast<TextEditor*>(text_editor_);
    editor->Render("TextEditor");
    ImGui::End();
}

void EditorApp::render_status_bar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float height = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));

    if (ImGui::Begin("StatusBar", nullptr, flags)) {
        ImGui::PopStyleVar(2);

        TextEditor* editor = static_cast<TextEditor*>(text_editor_);
        auto pos = editor->GetCursorPosition();

        // Detect language from file extension for display
        std::string lang_display = "Text";
        if (!current_file_.empty() && current_file_ != "untitled") {
            auto ext = std::filesystem::path(current_file_).extension().string();
            if (ext == ".c") lang_display = "C";
            else if (ext == ".cpp" || ext == ".hpp" || ext == ".cxx") lang_display = "C++";
            else if (ext == ".py") lang_display = "Python";
            else if (ext == ".js") lang_display = "JavaScript";
            else if (ext == ".lua") lang_display = "Lua";
            else if (ext == ".cs") lang_display = "C#";
            else if (ext == ".java") lang_display = "Java";
            else if (ext == ".html" || ext == ".htm") lang_display = "HTML";
            else if (ext == ".json") lang_display = "JSON";
            else if (ext == ".xml") lang_display = "XML";
            else if (ext == ".sql") lang_display = "SQL";
        }

        // Left: file name
        const char* fname = current_file_.c_str();
        ImGui::Text("%s%s", fname, dirty_ ? " *" : "");

        ImGui::SameLine();

        // Right: cursor position
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 220);
        ImGui::Text("Ln %d, Col %d", pos.mLine + 1, pos.mColumn + 1);

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 110);
        ImGui::Text("%s", lang_display.c_str());

        ImGui::End();
    } else {
        ImGui::PopStyleVar(2);
    }
}

void EditorApp::render_command_palette() {
    ImGui::OpenPopup("Command Palette");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 300));

    if (ImGui::BeginPopupModal("Command Palette", &show_command_palette_, ImGuiWindowFlags_NoResize)) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##cmd", command_buffer_, sizeof(command_buffer_), ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            show_command_palette_ = false;
        }

        // Simple command matching
        if (strlen(command_buffer_) > 0) {
            ImGui::Separator();
            if (ImGui::Selectable("File: Open")) {
                open_file("");
                show_command_palette_ = false;
                command_buffer_[0] = '\0';
            }
            if (ImGui::Selectable("File: Save")) {
                save_file();
                show_command_palette_ = false;
                command_buffer_[0] = '\0';
            }
            if (ImGui::Selectable("View: Toggle Theme")) {
                // Cycle between dark/light
                static bool dark = true;
                dark = !dark;
                dark ? ImGui::StyleColorsDark() : ImGui::StyleColorsLight();
                show_command_palette_ = false;
                command_buffer_[0] = '\0';
            }
        }

        ImGui::EndPopup();
    }
}

void EditorApp::open_file(const std::string& path) {
    std::string selected_path = path;

    // If no path given, show native file dialog
    if (path.empty()) {
        nfdchar_t* out_path = nullptr;
        nfdresult_t result = NFD_OpenDialog(&out_path, nullptr, 0, nullptr);

        if (result == NFD_OKAY && out_path) {
            selected_path = out_path;
            NFD_FreePath(out_path);
        } else {
            return; // User cancelled or error
        }
    }

    // Read the file
    std::ifstream file(selected_path);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open: %s\n", selected_path.c_str());
        return;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    file.close();

    // Set editor content
    TextEditor* editor = static_cast<TextEditor*>(text_editor_);
    editor->SetText(content);
    current_file_ = selected_path;
    dirty_ = false;

    // Auto-detect language from extension
    auto ext = std::filesystem::path(selected_path).extension().string();
    if (ext == ".c" || ext == ".h") {
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::C());
    } else if (ext == ".cpp" || ext == ".hpp" || ext == ".cxx" || ext == ".hxx") {
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    } else if (ext == ".py" || ext == ".lua") {
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    } else if (ext == ".js" || ext == ".json") {
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua()); // closest match
    } else if (ext == ".cs") {
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus()); // closest match
    } else if (ext == ".java") {
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus()); // closest match
    } else if (ext == ".html" || ext == ".htm" || ext == ".xml") {
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua()); // closest match
    } else if (ext == ".sql") {
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::SQL());
    } else if (ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".hlsl") {
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    }
    // else: keep current language definition
}

void EditorApp::save_file() {
    if (current_file_ == "untitled") return;

    TextEditor* editor = static_cast<TextEditor*>(text_editor_);
    std::ofstream file(current_file_);
    if (file.is_open()) {
        file << editor->GetText();
        dirty_ = false;
    }
}

void EditorApp::new_file() {
    current_file_ = "untitled";
    dirty_ = false;
    TextEditor* editor = static_cast<TextEditor*>(text_editor_);
    editor->SetText("");
}
