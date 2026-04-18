#!/usr/bin/env bash
# ============================================================================
# Build script for Linux (Wayland/X11)
# Usage: ./scripts/build-linux.sh [Release|Debug] [vcpkg|system|auto]
# ============================================================================

set -e

BUILD_TYPE="${1:-Release}"
DEP_MODE="${3:-auto}"  # auto, vcpkg, or system
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build-linux"

echo "============================================"
echo "  Building pcode-editor for Linux"
echo "============================================"
echo "Build type: $BUILD_TYPE"
echo "Dependency mode: $DEP_MODE"
echo ""

# Detect distro
if [ -f /etc/arch-release ]; then
    DISTRO="arch"
elif [ -f /etc/fedora-release ]; then
    DISTRO="fedora"
elif [ -f /etc/debian_version ]; then
    DISTRO="debian"
else
    DISTRO="unknown"
fi
echo "📍 Detected distro: $DISTRO"

# Check base dependencies
echo "📦 Checking dependencies..."
for cmd in cmake make pkg-config; do
    if ! command -v $cmd &> /dev/null; then
        echo "❌ $cmd not found. Install with your package manager"
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
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" \
        -DGLFW_BUILD_WAYLAND=ON \
        -DGLFW_BUILD_X11=OFF
else
    # Verify native deps based on distro
    MISSING_DEPS=()
    
    case "$DISTRO" in
        arch)
            for pkg in wayland libxkbcommon gtk3; do
                if ! pacman -Qs "^$pkg$" &> /dev/null; then
                    MISSING_DEPS+=($pkg)
                fi
            done
            ;;
        fedora|debian)
            for pkg in wayland-client wayland-cursor wayland-egl xkbcommon gtk+-3.0; do
                if ! pkg-config --exists $pkg 2>/dev/null; then
                    MISSING_DEPS+=($pkg)
                fi
            done
            ;;
        *)
            # Generic check
            for pkg in wayland-client gtk+-3.0; do
                if ! pkg-config --exists $pkg 2>/dev/null; then
                    MISSING_DEPS+=($pkg)
                fi
            done
            ;;
    esac

    if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
        echo "❌ Missing dependencies:"
        echo "   ${MISSING_DEPS[*]}"
        echo ""
        echo "Install with (Arch):"
        echo "   sudo pacman -S wayland libxkbcommon gtk3"
        echo ""
        echo "Install with (Ubuntu/Debian):"
        echo "   sudo apt install libwayland-dev wayland-protocols libxkbcommon-dev libgtk-3-dev"
        echo ""
        echo "Install with (Fedora):"
        echo "   sudo dnf install wayland-devel libxkbcommon-devel gtk3-devel"
        exit 1
    fi

    # Clean and configure with native deps
    echo "🔧 Configuring with native deps..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    cmake "$PROJECT_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DGLFW_BUILD_WAYLAND=ON \
        -DGLFW_BUILD_X11=OFF
fi

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
