#!/usr/bin/env bash
#
# Minimize a crash input to smallest reproducing case
#
# Usage: ./scripts/minimize_crash.sh <crash-file> [fuzzer-name]
#
# Examples:
#   ./scripts/minimize_crash.sh build-fuzz/crash-1234 bitmap_api

set -e

CRASH_FILE="$1"
FUZZER_NAME="${2:-bitmap_api}"

if [ -z "$CRASH_FILE" ]; then
    echo "Usage: $0 <crash-file> [fuzzer-name]"
    exit 1
fi

if [ ! -f "$CRASH_FILE" ]; then
    echo "Error: Crash file not found: $CRASH_FILE"
    exit 1
fi

BUILD_DIR="build-fuzz"
FUZZER_PATH="$BUILD_DIR/tests/fuzz/fuzz_$FUZZER_NAME"

if [ ! -f "$FUZZER_PATH" ]; then
    echo "Error: Fuzzer not found: $FUZZER_PATH"
    exit 1
fi

echo "Minimizing crash with fuzzer: fuzz_$FUZZER_NAME"
echo "Input: $CRASH_FILE"
echo ""

cd "$BUILD_DIR"
"./tests/fuzz/fuzz_$FUZZER_NAME" -minimize_crash=1 "../$CRASH_FILE"

echo ""
echo "✓ Minimization complete"
echo ""
echo "Minimized crash file: minimized-from-*"
echo "Original size: $(stat -f%z "$CRASH_FILE" 2>/dev/null || stat -c%s "$CRASH_FILE")"
ls -lh minimized-from-* 2>/dev/null || true
