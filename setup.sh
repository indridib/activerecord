#!/bin/bash
# Setup script for ActiveRecord C++ project (Linux/macOS)
# This script initializes vcpkg and bootstraps it if needed

set -e

echo "=== ActiveRecord Setup ==="
echo ""

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VCPKG_PATH="$PROJECT_ROOT/vcpkg"

# Check if vcpkg submodule exists
if [ ! -d "$VCPKG_PATH/.git" ]; then
    echo "Initializing vcpkg submodule..."
    git submodule update --init --recursive
fi

# Check if vcpkg is bootstrapped
if [ ! -f "$VCPKG_PATH/vcpkg" ]; then
    echo "Bootstrapping vcpkg..."
    cd "$VCPKG_PATH"
    ./bootstrap-vcpkg.sh
    cd "$PROJECT_ROOT"
    echo "vcpkg bootstrapped successfully!"
else
    echo "vcpkg already bootstrapped"
fi

echo ""
echo "=== Setup Complete! ==="
echo ""
echo "Next steps:"
echo "  1. Configure: cmake -B build"
echo "  2. Build:     cmake --build build --config Release"
echo "  3. Test:      ctest --test-dir build --output-on-failure"
echo ""
echo "Dependencies will be automatically installed via vcpkg.json manifest"
echo ""
