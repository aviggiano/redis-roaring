#!/usr/bin/env bash
#
# Build fuzzing targets for redis-roaring
#
# Usage: ./scripts/build_fuzzers.sh

set -e

echo "Building fuzzers with Clang..."

# Check if clang is available
if ! command -v clang &> /dev/null; then
    echo "Error: clang compiler not found"
    echo "Please install clang: sudo apt-get install clang"
    exit 1
fi

# Create build directory
BUILD_DIR="build-fuzz"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with fuzzing enabled
echo "Configuring CMake..."
CC=clang CXX=clang++ cmake -DENABLE_FUZZING=ON ..

# Build
echo "Building..."
make -j$(nproc)

echo ""
echo "✓ Fuzzers built successfully!"
echo ""
echo "Fuzz targets are available at:"
echo "  - $BUILD_DIR/tests/fuzz/fuzz_bitmap_api"
echo "  - $BUILD_DIR/tests/fuzz/fuzz_bitmap64_api"
echo "  - $BUILD_DIR/tests/fuzz/fuzz_bitmap_operations"
echo "  - $BUILD_DIR/tests/fuzz/fuzz_bitmap_serialization"
echo ""
echo "Run a fuzzer with:"
echo "  cd $BUILD_DIR"
echo "  ./tests/fuzz/fuzz_bitmap_api -max_total_time=60"
echo ""
echo "Or use: ./scripts/run_fuzzer.sh <fuzzer_name>"
