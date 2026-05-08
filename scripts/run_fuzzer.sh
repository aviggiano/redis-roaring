#!/usr/bin/env bash
#
# Run a specific fuzzer with reasonable defaults
#
# Usage: ./scripts/run_fuzzer.sh <fuzzer_name> [duration_seconds] [extra_args...]
#
# Examples:
#   ./scripts/run_fuzzer.sh bitmap_api
#   ./scripts/run_fuzzer.sh bitmap_api 300
#   ./scripts/run_fuzzer.sh bitmap64_api 60 -jobs=4

set -e

FUZZER_NAME="${1:-bitmap_api}"
DURATION="${2:-60}"
shift 2 || true
EXTRA_ARGS="$@"

BUILD_DIR="build-fuzz"
FUZZER_PATH="$BUILD_DIR/tests/fuzz/fuzz_$FUZZER_NAME"
CORPUS_DIR="$BUILD_DIR/tests/fuzz/corpus/$FUZZER_NAME"
RUNTIME_CORPUS_DIR="tests/fuzz/corpus/$FUZZER_NAME"

# Check if fuzzer exists
if [ ! -f "$FUZZER_PATH" ]; then
    echo "Error: Fuzzer not found at $FUZZER_PATH"
    echo ""
    echo "Available fuzzers:"
    echo "  - bitmap_api"
    echo "  - bitmap64_api"
    echo "  - bitmap_operations"
    echo "  - bitmap_serialization"
    echo "  - bitop_keys"
    echo "  - command_metadata"
    echo "  - command_dispatch"
    echo "  - cluster_routing"
    echo "  - persistence_sequences"
    echo "  - r_vs_r64_parity"
    echo ""
    echo "Build fuzzers first with: ./scripts/build_fuzzers.sh"
    exit 1
fi

# Create corpus directory if it doesn't exist
mkdir -p "$CORPUS_DIR"

echo "Running fuzzer: fuzz_$FUZZER_NAME"
echo "Duration: ${DURATION}s"
echo "Corpus: $CORPUS_DIR"
echo "Extra args: $EXTRA_ARGS"
echo ""

# Run the fuzzer
cd "$BUILD_DIR"
"./tests/fuzz/fuzz_$FUZZER_NAME" \
    -max_total_time="$DURATION" \
    -print_final_stats=1 \
    -rss_limit_mb=4096 \
    "$RUNTIME_CORPUS_DIR" \
    $EXTRA_ARGS

echo ""
echo "✓ Fuzzing completed"
echo ""
echo "Corpus saved to: $CORPUS_DIR"
echo "If crashes were found, they are saved as: crash-*"
