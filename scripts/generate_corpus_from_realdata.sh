#!/usr/bin/env bash
#
# Generate corpus seeds from CRoaring real benchmark data
#
# This script converts census1881 data into corpus seeds for fuzzing
#
# Usage: ./scripts/generate_corpus_from_realdata.sh

set -e

REALDATA_DIR="deps/CRoaring/benchmarks/realdata"
CENSUS_FILE="$REALDATA_DIR/census1881"
CORPUS_DIR="tests/fuzz/corpus"

echo "Generating corpus from real data..."

# Check if census data exists
if [ ! -f "$CENSUS_FILE" ]; then
    echo "Warning: Census data not found at $CENSUS_FILE"
    echo "Skipping real data corpus generation"
    exit 0
fi

echo "Found census data: $CENSUS_FILE"

# Create Python script to generate corpus
python3 << 'PYTHON_EOF'
import struct
import os

census_file = "deps/CRoaring/benchmarks/realdata/census1881"
corpus_base = "tests/fuzz/corpus"

# Read census data (it's a binary file with uint32 integers)
try:
    with open(census_file, "rb") as f:
        data = f.read()

    # Parse as uint32 array
    num_ints = len(data) // 4
    integers = struct.unpack(f"<{num_ints}I", data[:num_ints * 4])

    print(f"Loaded {num_ints} integers from census data")
    print(f"Range: {min(integers)} - {max(integers)}")

    # Generate various corpus seeds based on real data

    # Seed 1: Use first 100 integers for bitmap_api
    api_dir = os.path.join(corpus_base, "bitmap_api")
    os.makedirs(api_dir, exist_ok=True)

    seed_data = struct.pack("<B", 2)  # SETINTARRAY operation
    seed_data += struct.pack(f"<{min(100, num_ints)}I", *integers[:min(100, num_ints)])
    with open(os.path.join(api_dir, "seed_census_small"), "wb") as f:
        f.write(seed_data)

    # Seed 2: Use first 1000 integers
    seed_data = struct.pack("<B", 2)
    seed_data += struct.pack(f"<{min(1000, num_ints)}I", *integers[:min(1000, num_ints)])
    with open(os.path.join(api_dir, "seed_census_medium"), "wb") as f:
        f.write(seed_data)

    # Seed 3: 64-bit version with real data
    api64_dir = os.path.join(corpus_base, "bitmap64_api")
    os.makedirs(api64_dir, exist_ok=True)

    seed_data = struct.pack("<B", 2)
    # Convert to 64-bit
    integers_64 = [i for i in integers[:min(100, num_ints)]]
    seed_data += struct.pack(f"<{len(integers_64)}Q", *integers_64)
    with open(os.path.join(api64_dir, "seed_census_64bit"), "wb") as f:
        f.write(seed_data)

    # Seed 4: Operations fuzzer - two sets for AND/OR operations
    ops_dir = os.path.join(corpus_base, "operations")
    os.makedirs(ops_dir, exist_ok=True)

    # Take two disjoint sets
    set1_size = min(50, num_ints // 2)
    set2_size = min(50, num_ints // 2)

    seed_data = struct.pack("<BB", 2, 0)  # 2 inputs, AND operation
    seed_data += struct.pack(f"<{set1_size}I", *integers[:set1_size])
    seed_data += struct.pack(f"<{set2_size}I", *integers[num_ints//2:num_ints//2 + set2_size])
    with open(os.path.join(ops_dir, "seed_census_ops"), "wb") as f:
        f.write(seed_data)

    # Seed 5: Serialization fuzzer
    ser_dir = os.path.join(corpus_base, "serialization")
    os.makedirs(ser_dir, exist_ok=True)

    seed_data = struct.pack("<B", 0)  # int array test
    seed_data += struct.pack(f"<{min(200, num_ints)}I", *integers[:min(200, num_ints)])
    with open(os.path.join(ser_dir, "seed_census_serial"), "wb") as f:
        f.write(seed_data)

    print(f"\n✓ Generated corpus seeds from census data:")
    print(f"  - bitmap_api: seed_census_small, seed_census_medium")
    print(f"  - bitmap64_api: seed_census_64bit")
    print(f"  - operations: seed_census_ops")
    print(f"  - serialization: seed_census_serial")

except FileNotFoundError:
    print(f"Census file not found: {census_file}")
except Exception as e:
    print(f"Error processing census data: {e}")

PYTHON_EOF

echo ""
echo "✓ Corpus generation complete"
