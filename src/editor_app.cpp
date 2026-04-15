#include "editor_app.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <TextEditor.h>

#include <cstdio>
#include <fstream>
#include <sstream>

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
    ImGui::Begin("Editor");

    TextEditor* editor = static_cast<TextEditor*>(text_editor_);
    editor->Render("TextEditor");

    // Track dirty state
    // (TextEditor doesn't expose dirty state directly, so we track on edit)
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

        // Left: file name
        const char* fname = current_file_.c_str();
        ImGui::Text("%s%s", fname, dirty_ ? " *" : "");

        ImGui::SameLine();

        // Right: cursor position
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
        ImGui::Text("Ln %d, Col %d", pos.mLine + 1, pos.mColumn + 1);

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 100);
        ImGui::Text("C++");

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
    // For now, just set a placeholder. Full file dialog would use native dialogs.
    if (path.empty()) {
        current_file_ = "untitled";
        TextEditor* editor = static_cast<TextEditor*>(text_editor_);
        editor->SetText("// New file\n");
    } else {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream ss;
            ss << file.rdbuf();
            TextEditor* editor = static_cast<TextEditor*>(text_editor_);
            editor->SetText(ss.str());
            current_file_ = path;
            dirty_ = false;
        }
    }
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
