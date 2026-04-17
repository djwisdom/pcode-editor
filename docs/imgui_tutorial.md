# Dear ImGui Tutorial

A comprehensive guide to Dear ImGui for building immediate mode graphical user interfaces.

## Version

This tutorial covers **Dear ImGui v1.90+**.

---

## What is Dear ImGui?

Dear ImGui is a bloat-free immediate mode graphical user interface toolkit. It outputs optimized vertex buffers that you can render using any GPU renderer. It works with multiple platforms and graphics APIs.

### Key Characteristics

| Feature | Description |
|---------|-------------|
| **Immediate Mode** | UI is rebuilt every frame |
| **Portable** | Windows, macOS, Linux, BSD, Web |
| **No Dependencies** | Self-contained |
| **JSON-based Theming** | Easy customization |

---

## Getting Started

### Setup

```cpp
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

int main() {
    // Initialize GLFW
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui App", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Your UI here
        ImGui::ShowDemoWindow();

        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
```

---

## Basic Widgets

### Text

```cpp
// Simple text
ImGui::Text("Hello, World!");

// Colored text
ImGui::TextColored(ImVec4(1, 0, 0, 1), "Red text");

// Wrapped text
ImGui::TextWrapped("This text wraps automatically at the window width.");

// Labeled text (same line)
ImGui::LabelText("Label", "Value");
```

### Buttons

```cpp
// Simple button
if (ImGui::Button("Click Me")) {
    // Button clicked
}

// Small button
ImGui::SmallButton("Small");

// Arrow buttons
ImGui::ArrowButton("left", ImGuiDir_Left);
ImGui::ArrowButton("right", ImGuiDir_Right);

// Checkbox
static bool checkbox = false;
ImGui::Checkbox("Option", &checkbox);

// Radio button
static int radio = 0;
ImGui::RadioButton("Option A", &radio, 0);
ImGui::RadioButton("Option B", &radio, 1);
```

### Input

```cpp
// Text input
static char text[64] = "";
ImGui::InputText("Text", text, sizeof(text));

// Multiline text
static char multiline[256] = "";
ImGui::InputTextMultiline("Text", multiline, sizeof(multiline), ImVec2(0, 100));

// Integer input
static int integer = 0;
ImGui::InputInt("Int", &integer);

// Float input
static float floating = 0.0f;
ImGui::InputFloat("Float", &floating, 0.1f, 1.0f);

// Slider
static float slider = 0.5f;
ImGui::SliderFloat("Slider", &slider, 0.0f, 1.0f);

// Drag
static float drag = 0.0f;
ImGui::DragFloat("Drag", &drag, 0.01f);

// Color picker
static float color[4] = {1, 1, 1, 1};
ImGui::ColorPicker4("Color", color);
```

### Selection

```cpp
// Combo (dropdown)
static int current = 0;
const char* items[] = {"Option 1", "Option 2", "Option 3"};
if (ImGui::Combo("Combo", &current, items, 3)) {
}

// List box
static int listbox = 0;
if (ImGui::ListBox("List", &listbox, items, 2)) {
}

// Selectable
static int selected = -1;
for (int i = 0; i < 3; i++) {
    if (ImGui::Selectable(items[i], selected == i)) {
        selected = i;
    }
}
```

---

## Layout

### Windows

```cpp
// Child window (frame)
ImGui::BeginChild("child", ImVec2(200, 200), true);
ImGui::Text("Child content");
ImGui::EndChild();

// Group
ImGui::BeginGroup();
// Widgets in group share position
ImGui::EndGroup();

// Same line (inline)
ImGui::Button("A");
ImGui::SameLine();
ImGui::Button("B");

// Indent
ImGui::Indent();
ImGui::Unindent();
```

### Separator and Spacing

```cpp
ImGui::Separator();
ImGui::SeparatorText("Section Title");
ImGui::Spacing();
ImGui::Dummy(ImVec2(10, 10));
```

### Tables (ImGui 1.82+)

```cpp
if (ImGui::BeginTable("table", 3, ImGuiTableFlags_Borders)) {
    ImGui::TableNextRow();
    
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Column 1");
    
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("Column 2");
    
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("Column 3");
    
    ImGui::EndTable();
}
```

### Docking (Advanced)

```cpp
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | 
    ImGuiWindowFlags_NoCollapse | 
    ImGuiWindowFlags_NoResize | 
    ImGuiWindowFlags_NoMove | 
    ImGuiWindowFlags_NoBringToFrontOnFocus | 
    ImGuiWindowFlags_NoNavFocus;

ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
ImGui::Begin("Dockspace", nullptr, window_flags | ImGuiWindowFlags_MenuBar);
ImGui::DockSpace(ImGui::GetID("Dockspace"));
ImGui::End();
```

---

## Menus

```cpp
// Menu bar
if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N")) {}
        if (ImGui::MenuItem("Open", "Ctrl+O")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {}
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
        if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

// Context menu (right-click)
if (ImGui::BeginPopupContextItem("context")) {
    if (ImGui::MenuItem("Option")) {}
    ImGui::EndPopup();
}
```

---

## Popups and Dialogs

### Modal Dialog

```cpp
static bool show_modal = false;
if (ImGui::Button("Open Modal")) {
    ImGui::OpenPopup("My Modal");
}

if (ImGui::BeginPopupModal("My Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("This is a modal dialog");
    if (ImGui::Button("Close")) {
        ImGui::ClosePopup("My Modal");
    }
    ImGui::EndPopup();
}
```

### Message Box

```cpp
ImGui::Text("Processing...");
if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("An error occurred!");
    if (ImGui::Button("OK")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
```

### File Dialog

```cpp
// Using native-file-dialog-extended
#include "nfd.h"

if (ImGui::Button("Open File")) {
    nfdchar_t* outPath = NULL;
    nfdresult_t result = NFD_OpenDialog("txt", NULL, &outPath);
    if (result == NFD_OKAY) {
        // Handle file path
        NFD_Free(outPath);
    }
}
```

---

## Graphs and Visualization

### Progress Bar

```cpp
static float progress = 0.0f;
ImGui::ProgressBar(progress, ImVec2(0, 0), "Label");
progress += 0.01f;
if (progress > 1.0f) progress = 0;
```

### Plot

```cpp
// Requires implot (separate library)
// #include "implot.h"
```

### Color Wheel

```cpp
static float col[3] = {1, 0, 1};
ImGui::ColorPicker3("Color", col);
```

---

## Themes and Styling

### Built-in Themes

```cpp
// Dark theme (default)
ImGui::StyleColorsDark();

// Light theme
ImGui::StyleColorsLight();

// Classic theme
ImGui::StyleColorsClassic();
```

### Custom Styling

```cpp
ImGuiStyle& style = ImGui::GetStyle();

// Colors
style.Colors[ImGuiCol_Text] = ImVec4(1, 1, 1, 1);
style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1);

// Rounding
style.WindowRounding = 5.0f;
style.FrameRounding = 3.0f;

// Padding
style.WindowPadding = ImVec2(10, 10);
style.FramePadding = ImVec2(5, 5);
```

---

## Performance Tips

### ID Management

```cpp
// Bad: Multiple widgets with same ID
for (int i = 0; i < 10; i++) {
    ImGui::Text("Item");  // Same ID each iteration!
}

// Good: Unique IDs using i
for (int i = 0; i < 10; i++) {
    ImGui::PushID(i);
    ImGui::Text("Item %d", i);
    ImGui::PopID();
}
```

### Scoped Operations

```cpp
// Widgets
ImGui::PushID("widget");  // Unique ID stack
// ... widgets ...
ImGui::PopID();

// Style
ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
// ... widgets ...
ImGui::PopStyleVar();

// Colors
ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0, 0, 1));
// ... widgets ...
ImGui::PopStyleColor();
```

### Draw Lists (Custom Rendering)

```cpp
ImDrawList* draw_list = ImGui::GetWindowDrawList();
ImVec2 p = ImGui::GetCursorScreenPos();
draw_list->AddRectFilled(p, ImVec2(p.x + 100, p.y + 100), IM_COL32(255, 0, 0, 255));
```

---

## Keyboard and Mouse Input

### Keyboard

```cpp
ImGuiIO& io = ImGui::GetIO();

// Is key pressed (one shot)
if (ImGui::IsKeyPressed(ImGuiKey_A)) {
    // A pressed
}

// Is key down (continuous)
if (io.KeysDown[ImGuiKey_A]) {
    // A is held
}

// Key modifiers
if (io.KeyCtrl) {}
if (io.KeyShift) {}
if (io.KeyAlt) {}
if (io.KeySuper) {}
```

### Mouse

```cpp
ImGuiIO& io = ImGui::GetIO();

// Is mouse clicked
if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {}

// Is mouse down
if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {}

// Mouse position
ImVec2 mouse_pos = ImGui::GetMousePos();

// Mouse delta
ImVec2 mouse_delta = ImGui::GetMouseDragDelta();
```

---

## Font Handling

### Loading Fonts

```cpp
ImGuiIO& io = ImGui::GetIO();
io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16.0f);
io.Fonts->Build();
```

### Multiple Fonts

```cpp
ImFont* font1 = io.Fonts->AddFontFromFileTTF("font1.ttf", 14);
ImFont* font2 = io.Fonts->AddFontFromFileTTF("font2.ttf", 16);

// Use specific font
ImGui::PushFont(font1);
ImGui::Text("Using font 1");
ImGui::PopFont();
```

---

## Internationalization

### UTF-8 Support

```cpp
// Ensure source file is UTF-8 encoded
// Set ImGuiIO to use UTF-8
ImGuiIO& io = ImGui::GetIO();
io.ImeWindowHandle = hwnd;  // Windows IME handle
```

---

## Common Issues

### Fonts Not Rendering

- Call `io.Fonts->Build()` after adding fonts
- Ensure font file path is correct
- Check font file is not corrupted

### Window Not Showing

- Call `ImGui::NewFrame()` before UI code
- Call `ImGui::Render()` after UI code

### Layout Issues

- Wrap long sections in `BeginChild()/EndChild()`
- Use `SameLine()` for horizontal layout
- Use `Spacing()` for gaps

---

## Resources

- [Dear ImGui GitHub](https://github.com/ocornut/imgui)
- [ImGui Discord](https://discord.gg/immediate-mode-gui)
- [ImGui Wiki](https://github.com/ocornut/imgui/wiki)

---

## License

This tutorial is for educational purposes. Dear ImGui itself is licensed under MIT.
See [ImGui LICENSE](https://github.com/ocornut/imgui/blob/master/LICENSE) for details.