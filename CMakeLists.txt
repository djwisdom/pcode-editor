# ============================================================================
# Code Editor — C++ GUI Text Editor (Dear ImGui + GLFW)
# Cross-platform: Windows (Win32), Linux (Wayland), BSD (X11), macOS
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(pcode-editor VERSION 0.2.95 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Suppress deprecated function warnings (sprintf, strncpy, getenv, localtime)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATION)

# Static runtime for portable builds (no VC++ redistributable needed)
if(PLATFORM_WINDOWS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
    add_compile_options(/MT)
endif()

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
# Dependency Resolution: vcpkg preferred, FetchContent fallback
# ============================================================================
include(FetchContent)

# Try vcpkg first (if installed)
find_package(GLFW QUIET)
find_package(imgui QUIET)
find_package(nativefiledialog-extended QUIET)

# If vcpkg not found, use FetchContent (automatic download)
if(NOT TARGET imgui::imgui)
    message(STATUS "Using FetchContent for dependencies (vcpkg not found)")
    
    # Dear ImGui — immediate mode GUI
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        docking  # docking branch (stable, includes docking)
    )
    FetchContent_MakeAvailable(imgui)
endif()

if(NOT TARGET GLFW::GLFW)
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
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    elseif(PLATFORM_LINUX)
        set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    endif()
    
    FetchContent_MakeAvailable(glfw)
endif()

if(NOT TARGET nfd::nfd)
    # Native File Dialog — cross-platform native file picker
    FetchContent_Declare(
        nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG        master
    )
    set(NFD_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nfd)
endif()

# Zep — embeddable code editor with word wrap support
# NOTE: Zep v0.1.12 requires significant API changes to integrate properly.
# For now using ImGuiColorTextEdit (no word wrap).
set(zep_SOURCE_DIR "build/_deps/zep-0.1.12" CACHE PATH "Zep source (not integrated)")

# ImGuiColorTextEdit — syntax highlighting
FetchContent_Declare(
    imgui_coloredit
    GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(imgui_coloredit)

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
    ${APP_SOURCES}
)

# Link as Windows subsystem (no console)
target_link_options(${PROJECT_NAME} PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)

# Determine include directories based on dependency source (vcpkg or FetchContent)
if(TARGET imgui::imgui)
    # vcpkg imgui - use imported targets
    target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
    # imgui vcpkg includes imgui_impl_* in same dir, need to include backends differently
    if(imgui_FOUND)
        get_target_property(IMGUI_INCLUDE imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_INCLUDE})
    endif()
else()
    # FetchContent imgui - use source dir
    target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR})
    if(EXISTS ${imgui_SOURCE_DIR}/backends)
        target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR}/backends)
    endif()
    
    # Add imgui sources from FetchContent
    target_sources(${PROJECT_NAME} PRIVATE
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${imgui_coloredit_SOURCE_DIR}
    ${nfd_SOURCE_DIR}/src/include
)

# Add ImGuiColorTextEdit source
target_sources(${PROJECT_NAME} PRIVATE
    ${imgui_coloredit_SOURCE_DIR}/TextEditor.cpp
)

# ============================================================================
# Platform-specific libraries and configuration
# ============================================================================
if(PLATFORM_WINDOWS)
    # Windows: Win32 API
    if(TARGET GLFW::GLFW)
        target_link_libraries(${PROJECT_NAME} PRIVATE GLFW::GLFW)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 gdi32)
    
    if(TARGET nfd::nfd)
        target_link_libraries(${PROJECT_NAME} PRIVATE nfd::nfd)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE nfd)
    endif()
    message(STATUS "Linking for Windows (Win32)")
    
elseif(PLATFORM_MACOS)
    # macOS: Cocoa + OpenGL
    if(TARGET GLFW::GLFW)
        target_link_libraries(${PROJECT_NAME} PRIVATE GLFW::GLFW)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    target_link_libraries(${PROJECT_NAME} PRIVATE nfd::nfd)
    message(STATUS "Linking for macOS")
    
elseif(PLATFORM_LINUX)
    # Linux: Wayland + GTK3 for native file dialog
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-cursor wayland-egl xkbcommon)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES} dl)
    if(TARGET GLFW::GLFW)
        target_link_libraries(${PROJECT_NAME} PRIVATE GLFW::GLFW)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
    endif()
    if(TARGET nfd::nfd)
        target_link_libraries(${PROJECT_NAME} PRIVATE nfd::nfd)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE nfd)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GTK3_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${GTK3_INCLUDE_DIRS} ${WAYLAND_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
    message(STATUS "Linking for Linux (Wayland)")
    
elseif(PLATFORM_BSD)
    # BSD: X11 + GTK3 for native file dialog
    find_package(OpenGL REQUIRED)
    find_package(X11 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    
    target_link_directories(${PROJECT_NAME} PRIVATE /usr/local/lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES} dl)
    if(TARGET GLFW::GLFW)
        target_link_libraries(${PROJECT_NAME} PRIVATE GLFW::GLFW)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
    endif()
    if(TARGET nfd::nfd)
        target_link_libraries(${PROJECT_NAME} PRIVATE nfd::nfd)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE nfd)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GTK3_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIRS} ${GTK3_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
    message(STATUS "Linking for BSD (X11)")
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-unused-parameter>
)

# ============================================================================
# Install
# ============================================================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# ============================================================================
# Test Suite
# ============================================================================
enable_testing()

add_executable(test_view_features tests/test_view_features.cpp)
target_compile_options(test_view_features PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)

add_test(NAME ViewFeatures COMMAND test_view_features)

# Regression test suite
add_executable(test_regression tests/test_regression.cpp)
target_compile_options(test_regression PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)

add_test(NAME Regression COMMAND test_regression)

# Custom target to run tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_view_features test_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Custom target to run ONLY regression tests
add_custom_target(test_regression_only
    COMMAND test_regression
    DEPENDS test_regression
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
