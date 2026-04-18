#!/usr/bin/env bash
# ============================================================================
# Build script for macOS (Cocoa + OpenGL)
# Usage: ./scripts/build-macos.sh [Release|Debug] [vcpkg|system|auto]
# ============================================================================

set -e

BUILD_TYPE="${1:-Release}"
DEP_MODE="${3:-auto}"  # auto, vcpkg, or system
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build-macos"

echo "============================================"
echo "  Building pcode-editor for macOS"
echo "============================================"
echo "Build type: $BUILD_TYPE"
echo "Dependency mode: $DEP_MODE"
echo ""

# Check base dependencies
echo "📦 Checking dependencies..."
for cmd in cmake make; do
    if ! command -v $cmd &> /dev/null; then
        echo "❌ $cmd not found. Install with Homebrew:"
        echo "   brew install cmake"
        exit 1
    fi
done

# Try vcpkg if requested or auto
USE_VCPKG=false
if [ "$DEP_MODE" = "vcpkg" ] || [ "$DEP_MODE" = "auto" ]; then
    if command -v vcpkg &> /dev/null || [ -d "$HOME/vcpkg" ]; then
        USE_VCPKG=true
        echo "✅ Using vcpkg"
    fi
fi

if [ "$USE_VCPKG" = "true" ]; then
    echo "🔧 Configuring with vcpkg..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    VCPKG_DIR="$HOME/vcpkg"
    [ -d "$VCPKG_DIR" ] || VCPKG_DIR="/usr/local/vcpkg"
    
    cmake "$PROJECT_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
else
    # Verify native deps (Homebrew)
    MISSING_DEPS=()
    for pkg in glfw imgui; do
        if ! brew list $pkg &> /dev/null; then
            MISSING_DEPS+=($pkg)
        fi
    done
    
    if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
        echo "❌ Missing dependencies:"
        echo "   ${MISSING_DEPS[*]}"
        echo ""
        echo "Install with Homebrew:"
        echo "   brew install ${MISSING_DEPS[*]}"
        # Note: NFD not available via brew, use FetchContent or compile manually
    fi

    # Clean and configure with native deps
    echo "🔧 Configuring with native deps..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    cmake "$PROJECT_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
fi

# Build
echo "🔨 Building..."
make -j$(sysctl -n hw.ncpu 2>/dev/null || echo 2)

# Verify
echo ""
echo "============================================"
if [ -f "pcode-editor" ]; then
    echo "✅ Build successful!"
    echo "📍 Executable: $BUILD_DIR/pcode-editor"
    file pcode-editor
    ls -lh pcode-editor
else
    echo "❌ Build failed!"
    exit 1
fi
echo "============================================"