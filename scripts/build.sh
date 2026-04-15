#!/usr/bin/env bash
# ============================================================================
# Universal build script - auto-detects platform and builds accordingly
# Usage: ./scripts/build.sh [Release|Debug]
# ============================================================================

set -e

BUILD_TYPE="${1:-Release}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Detect platform
PLATFORM="$(uname -s)"

case "$PLATFORM" in
    FreeBSD|OpenBSD|NetBSD)
        echo "🖥️  Detected BSD platform"
        exec "$SCRIPT_DIR/build-freebsd.sh" "$BUILD_TYPE"
        ;;
    Linux)
        echo "🖥️  Detected Linux platform"
        exec "$SCRIPT_DIR/build-linux.sh" "$BUILD_TYPE"
        ;;
    Darwin)
        echo "🖥️  Detected macOS platform"
        echo "🔧 Building for macOS..."
        BUILD_DIR="$PROJECT_DIR/build-macos"
        rm -rf "$BUILD_DIR"
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        make -j$(sysctl -n hw.ncpu)
        echo "✅ Build complete: $BUILD_DIR/pcode-editor"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        echo "🖥️  Detected Windows platform (MSYS2/MinGW)"
        echo "🔧 Building for Windows..."
        BUILD_DIR="$PROJECT_DIR/build-windows-mingw"
        rm -rf "$BUILD_DIR"
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake "$PROJECT_DIR" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        make -j$(nproc)
        echo "✅ Build complete: $BUILD_DIR/pcode-editor.exe"
        ;;
    *)
        echo "❌ Unsupported platform: $PLATFORM"
        echo "Supported platforms: FreeBSD, OpenBSD, NetBSD, Linux, macOS, Windows (MSYS2/MinGW)"
        exit 1
        ;;
esac
