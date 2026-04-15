# ============================================================================
# Code Editor — C++ GUI Text Editor (Dear ImGui + GLFW)
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(pcode-editor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Dependencies (fetched automatically via FetchContent)
# ============================================================================
include(FetchContent)

# GLFW — cross-platform window/input
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Dear ImGui — immediate mode GUI
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking  # docking branch (stable, includes docking)
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# ImGuiColorTextEdit — syntax highlighting, line numbers, minimap
FetchContent_Declare(
    imgui_coloredit
    GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit.git
    GIT_TAG        master
)
FetchContent_GetProperties(imgui_coloredit)
if(NOT imgui_coloredit_POPULATED)
    FetchContent_Populate(imgui_coloredit)
endif()

# ============================================================================
# Dear ImGui sources
# ============================================================================
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# ============================================================================
# ImGuiColorTextEdit sources
# ============================================================================
set(TEXTEDIT_SOURCES
    ${imgui_coloredit_SOURCE_DIR}/TextEditor.cpp
)

# ============================================================================
# Application sources
# ============================================================================
set(APP_SOURCES
    src/main.cpp
    src/editor_app.cpp
    src/editor_app.h
)

# ============================================================================
# Executable
# ============================================================================
add_executable(${PROJECT_NAME}
    ${IMGUI_SOURCES}
    ${TEXTEDIT_SOURCES}
    ${APP_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_coloredit_SOURCE_DIR}
    ${GLFW_INCLUDE_DIR}
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw opengl32 gdi32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw ${OPENGL_LIBRARIES} dl)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)

# ============================================================================
# Install
# ============================================================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
