#!/usr/bin/env bash
#
# Reproduce a crash from a crash file
#
# Usage: ./scripts/reproduce_crash.sh <crash-file>
#
# Examples:
#   ./scripts/reproduce_crash.sh build-fuzz/crash-1234567890abcdef

set -e

CRASH_FILE="$1"

if [ -z "$CRASH_FILE" ]; then
    echo "Usage: $0 <crash-file>"
    echo ""
    echo "Available crash files:"
    ls -1 build-fuzz/crash-* 2>/dev/null || echo "  (none found)"
    exit 1
fi

if [ ! -f "$CRASH_FILE" ]; then
    echo "Error: Crash file not found: $CRASH_FILE"
    exit 1
fi

# Try to determine which fuzzer from the crash file location or name
# For now, try all fuzzers
BUILD_DIR="build-fuzz"

echo "Attempting to reproduce crash: $CRASH_FILE"
echo ""

FUZZERS=("fuzz_bitmap_api" "fuzz_bitmap64_api" "fuzz_bitmap_operations" "fuzz_bitmap_serialization")

for fuzzer in "${FUZZERS[@]}"; do
    FUZZER_PATH="$BUILD_DIR/tests/fuzz/$fuzzer"

    if [ -f "$FUZZER_PATH" ]; then
        echo "Trying with $fuzzer..."
        cd "$BUILD_DIR"

        if "./tests/fuzz/$fuzzer" "../$CRASH_FILE" 2>&1 | grep -q "ERROR:"; then
            echo ""
            echo "✓ Crash reproduced with: $fuzzer"
            echo ""
            echo "Debug with:"
            echo "  gdb --args ./$FUZZER_PATH ../$CRASH_FILE"
            echo ""
            echo "Minimize with:"
            echo "  ./scripts/minimize_crash.sh $CRASH_FILE $fuzzer"
            exit 0
        fi

        cd - > /dev/null
    fi
done

echo ""
echo "Could not reproduce crash with any fuzzer"
echo "The crash might have been fixed, or it's a non-deterministic issue"
