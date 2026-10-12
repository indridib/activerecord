#!/bin/bash
# Quick build script for ActiveRecord on Linux
# Usage: ./build.sh [Release|Debug]

set -e  # Exit on error

BUILD_TYPE="${1:-Release}"

echo "=== ActiveRecord Build Script ==="
echo ""

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VCPKG_PATH="$PROJECT_ROOT/vcpkg"

# Check if vcpkg exists and is bootstrapped
if [ ! -f "$VCPKG_PATH/vcpkg" ]; then
    echo "vcpkg not found or not bootstrapped. Running setup..."
    echo ""
    "$PROJECT_ROOT/setup.sh"
    echo ""
fi

echo "Using project vcpkg at: $VCPKG_PATH"
echo ""
echo "Note: Dependencies (SQLite3, GoogleTest) will be installed automatically"
echo "      via vcpkg.json manifest mode when CMake configures"
echo ""

# Configure with vcpkg toolchain (auto-detected by CMakeLists.txt)
echo "Configuring CMake..."
cmake -B build \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DAR_BUILD_TESTS=ON \
  -DAR_SQLITE=ON

# Build
echo ""
echo "Building ($BUILD_TYPE)..."
cmake --build build --config "$BUILD_TYPE"

# Test
echo ""
echo "Running tests..."
ctest --test-dir build -C "$BUILD_TYPE" --output-on-failure

if [ $? -eq 0 ]; then
    echo ""
    echo "All tests passed!"
else
    echo ""
    echo "WARNING: Some tests failed!"
fi

# Success!
echo ""
echo "=== Build Complete! ==="
echo ""
echo "Run the SQLite example:"
echo "  ./build/examples/sqlite_example"
echo ""
echo "Run tests manually:"
echo "  ctest --test-dir build -C $BUILD_TYPE -V"
echo ""
