#!/usr/bin/env bash
# ============================================================================
# Build script for Linux (Wayland)
# Usage: ./scripts/build-linux.sh [Release|Debug]
# ============================================================================

set -e

BUILD_TYPE="${1:-Release}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build-linux"

echo "============================================"
echo "  Building pcode-editor for Linux (Wayland)"
echo "============================================"
echo "Build type: $BUILD_TYPE"
echo ""

# Check dependencies
echo "📦 Checking dependencies..."
for cmd in cmake make pkg-config; do
    if ! command -v $cmd &> /dev/null; then
        echo "❌ $cmd not found. Install with your package manager"
        exit 1
    fi
done

# Verify Wayland dependencies
MISSING_DEPS=()
for pkg in wayland-client wayland-cursor wayland-egl xkbcommon gtk+-3.0; do
    if ! pkg-config --exists $pkg 2>/dev/null; then
        MISSING_DEPS+=($pkg)
    fi
done

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo "❌ Missing dependencies:"
    echo "   ${MISSING_DEPS[*]}"
    echo ""
    echo "Install with (Ubuntu/Debian):"
    echo "   sudo apt install libwayland-dev wayland-protocols libxkbcommon-dev libgtk-3-dev"
    echo ""
    echo "Install with (Fedora):"
    echo "   sudo dnf install wayland-devel libxkbcommon-devel gtk3-devel"
    echo ""
    echo "Install with (Arch):"
    echo "   sudo pacman -S wayland libxkbcommon gtk3"
    exit 1
fi

# Clean and configure
echo "🔧 Configuring CMake..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DGLFW_BUILD_WAYLAND=ON \
    -DGLFW_BUILD_X11=OFF

# Build
echo "🔨 Building..."
make -j$(nproc)

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
