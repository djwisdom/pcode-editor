# ============================================================================
# Code Editor — C++ GUI Text Editor (Dear ImGui + GLFW)
# Cross-platform: Windows (Win32), Linux (Wayland), BSD (X11), macOS
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(pcode-editor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Platform Detection
# ============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    set(PLATFORM_BSD ON)
    message(STATUS "Building for BSD platform")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_LINUX ON)
    message(STATUS "Building for Linux platform")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM_WINDOWS ON)
    message(STATUS "Building for Windows platform")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM_MACOS ON)
    message(STATUS "Building for macOS platform")
endif()

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

# Platform-specific GLFW configuration
if(PLATFORM_BSD)
    # BSD: X11 only
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    message(STATUS "GLFW: Building with X11 support for BSD")
elseif(PLATFORM_LINUX)
    # Linux: Wayland by default
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    message(STATUS "GLFW: Building with Wayland support for Linux")
endif()

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

# Native File Dialog — cross-platform native file picker
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        master
)
set(NFD_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nfd)

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
    ${nfd_SOURCE_DIR}/src/include
    ${GLFW_INCLUDE_DIR}
)

# ============================================================================
# Platform-specific libraries and configuration
# ============================================================================
if(PLATFORM_WINDOWS)
    # Windows: Win32 API
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw opengl32 gdi32 nfd)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
    message(STATUS "Linking for Windows (Win32)")
    
elseif(PLATFORM_MACOS)
    # macOS: Cocoa + OpenGL
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" nfd)
    message(STATUS "Linking for macOS")
    
elseif(PLATFORM_LINUX)
    # Linux: Wayland + GTK3 for native file dialog
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-cursor wayland-egl xkbcommon)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw ${OPENGL_LIBRARIES} dl nfd ${GTK3_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${GTK3_INCLUDE_DIRS} ${WAYLAND_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
    message(STATUS "Linking for Linux (Wayland)")
    
elseif(PLATFORM_BSD)
    # BSD: X11 + GTK3 for native file dialog
    find_package(OpenGL REQUIRED)
    find_package(X11 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    
    # Add /usr/local/lib to library search path (common on BSD)
    target_link_directories(${PROJECT_NAME} PRIVATE /usr/local/lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw ${OPENGL_LIBRARIES} dl nfd ${GTK3_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIRS} ${GTK3_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
    message(STATUS "Linking for BSD (X11)")
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)

# ============================================================================
# Install
# ============================================================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
