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
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FUZZER_SHORT="${FUZZER_NAME#fuzz_}"

if [ -z "$CRASH_FILE" ]; then
    echo "Usage: $0 <crash-file> [fuzzer-name]"
    exit 1
fi

if [ ! -f "$CRASH_FILE" ]; then
    echo "Error: Crash file not found: $CRASH_FILE"
    exit 1
fi

CRASH_FILE_ABS="$(cd "$(dirname "$CRASH_FILE")" && pwd)/$(basename "$CRASH_FILE")"
BUILD_DIR="$ROOT_DIR/build-fuzz"
FUZZER_PATH="$BUILD_DIR/tests/fuzz/fuzz_$FUZZER_SHORT"

if [ ! -f "$FUZZER_PATH" ]; then
    echo "Error: Fuzzer not found: $FUZZER_PATH"
    exit 1
fi

echo "Minimizing crash with fuzzer: fuzz_$FUZZER_SHORT"
echo "Input: $CRASH_FILE"
echo ""

cd "$BUILD_DIR"
"./tests/fuzz/fuzz_$FUZZER_SHORT" -minimize_crash=1 "$CRASH_FILE_ABS"

echo ""
echo "✓ Minimization complete"
echo ""
echo "Minimized crash file: minimized-from-*"
echo "Original size: $(stat -f%z "$CRASH_FILE_ABS" 2>/dev/null || stat -c%s "$CRASH_FILE_ABS")"
ls -lh minimized-from-* 2>/dev/null || true
