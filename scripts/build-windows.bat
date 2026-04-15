@echo off
REM ============================================================================
REM Build script for Windows (Win32/MSVC)
REM Usage: scripts\build-windows.bat [Release|Debug]
REM ============================================================================

setlocal enabledelayedexpansion

set BUILD_TYPE=%~1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo ============================================
echo   Building pcode-editor for Windows (Win32^)
echo ============================================
echo Build type: %BUILD_TYPE%
echo.

REM Check if cmake is available
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: cmake not found in PATH
    echo Install from: https://cmake.org/download/
    exit /b 1
)

REM Check if Visual Studio is available
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Visual Studio build tools not found
    echo Install Visual Studio Build Tools from: https://visualstudio.microsoft.com/downloads/
    exit /b 1
)

REM Clean and configure
echo Configuring CMake...
set BUILD_DIR=build-windows
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if %errorlevel% neq 0 (
    echo.
    echo ERROR: CMake configuration failed
    cd ..
    exit /b 1
)

REM Build
echo Building...
cmake --build . --config %BUILD_TYPE% --parallel %NUMBER_OF_PROCESSORS%
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Build failed
    cd ..
    exit /b 1
)

REM Verify
echo.
echo ============================================
if exist "%BUILD_TYPE%\pcode-editor.exe" (
    echo Build successful!
    echo Executable: %CD%\%BUILD_TYPE%\pcode-editor.exe
    dir "%BUILD_TYPE%\pcode-editor.exe"
) else (
    echo ERROR: Build failed - executable not found
    cd ..
    exit /b 1
)
echo ============================================

cd ..
