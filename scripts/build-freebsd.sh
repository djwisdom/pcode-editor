#!/usr/bin/env bash
# ============================================================================
# Build script for FreeBSD (X11)
# Usage: ./scripts/build-freebsd.sh [Release|Debug]
# ============================================================================

set -e

BUILD_TYPE="${1:-Release}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build-freebsd"

echo "============================================"
echo "  Building pcode-editor for FreeBSD (X11)"
echo "============================================"
echo "Build type: $BUILD_TYPE"
echo ""

# Check dependencies
echo "📦 Checking dependencies..."
for cmd in cmake make pkg-config; do
    if ! command -v $cmd &> /dev/null; then
        echo "❌ $cmd not found. Install with: pkg install cmake"
        exit 1
    fi
done

# Verify GTK3 is installed
if ! pkg-config --exists gtk+-3.0 2>/dev/null; then
    echo "❌ GTK3 not found. Install with:"
    echo "   sudo pkg install gtk3"
    exit 1
fi

# Clean and configure
echo "🔧 Configuring CMake..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DGLFW_BUILD_WAYLAND=OFF \
    -DGLFW_BUILD_X11=ON

# Build
echo "🔨 Building..."
BUILD_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 2)
make -j"$BUILD_JOBS"

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
