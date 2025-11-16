#!/usr/bin/env bash
#
# Run all fuzzers sequentially
#
# Usage: ./scripts/run_all_fuzzers.sh [duration_per_fuzzer]
#
# Examples:
#   ./scripts/run_all_fuzzers.sh        # 60 seconds per fuzzer
#   ./scripts/run_all_fuzzers.sh 300    # 5 minutes per fuzzer

set -e

DURATION="${1:-60}"

echo "=================================="
echo "Running all fuzzers"
echo "Duration per fuzzer: ${DURATION}s"
echo "=================================="
echo ""

FUZZERS=("bitmap_api" "bitmap64_api" "bitmap_operations" "bitmap_serialization")

for fuzzer in "${FUZZERS[@]}"; do
    echo "========================================"
    echo "Fuzzer: $fuzzer"
    echo "========================================"
    ./scripts/run_fuzzer.sh "$fuzzer" "$DURATION" || {
        echo "Warning: Fuzzer $fuzzer failed or found crashes"
    }
    echo ""
done

echo "=================================="
echo "All fuzzers completed"
echo "=================================="

# Check for crashes
if ls build-fuzz/crash-* 1> /dev/null 2>&1; then
    echo ""
    echo "⚠️  CRASHES FOUND:"
    ls -lh build-fuzz/crash-*
    echo ""
    echo "Reproduce with: ./scripts/reproduce_crash.sh <crash-file>"
    exit 1
else
    echo "✓ No crashes found"
fi
