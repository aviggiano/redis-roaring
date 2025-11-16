#!/usr/bin/env bash
#
# Generate corpus seeds from CRoaring real benchmark data
#
# This script converts census and other datasets into corpus seeds for fuzzing
#
# Usage: ./scripts/generate_corpus_from_realdata.sh

set -e

REALDATA_DIR="deps/CRoaring/benchmarks/realdata"
CORPUS_DIR="tests/fuzz/corpus"

echo "Generating corpus from real data..."

# Check if realdata directory exists
if [ ! -d "$REALDATA_DIR" ]; then
    echo "Warning: Realdata directory not found at $REALDATA_DIR"
    echo "Skipping real data corpus generation"
    exit 0
fi

# Create Python script to generate corpus
python3 << 'PYTHON_EOF'
import struct
import os
import glob
import random

realdata_dir = "deps/CRoaring/benchmarks/realdata"
corpus_base = "tests/fuzz/corpus"

# Helper to read comma-separated uint32 integers from text file
def read_integers_from_file(filepath):
    try:
        with open(filepath, 'r') as f:
            content = f.read().strip()
            if not content:
                return []
            integers = [int(x) for x in content.split(',') if x.strip()]
            return integers
    except Exception as e:
        print(f"  Error reading {filepath}: {e}")
        return []

# Use census1881 dataset (standard benchmark for CRoaring)
dataset_name = 'census1881'
dataset_path = os.path.join(realdata_dir, dataset_name)

print("Loading census1881 dataset...")

integers = []
if not os.path.exists(dataset_path):
    print(f"Error: {dataset_name} not found at {dataset_path}")
    exit(1)

txt_files = glob.glob(os.path.join(dataset_path, "*.txt"))
if not txt_files:
    print(f"Error: No txt files found in {dataset_path}")
    exit(1)

print(f"  Found {len(txt_files)} files")

# Load all files from census1881 for comprehensive corpus
for i, txt_file in enumerate(txt_files):
    if i % 20 == 0:
        print(f"  Processing file {i+1}/{len(txt_files)}...")
    file_integers = read_integers_from_file(txt_file)
    if file_integers:
        integers.extend(file_integers)

if not integers:
    print("Error: No integers loaded from census1881")
    exit(1)

print(f"✓ Loaded {len(integers)} integers (range: {min(integers)}-{max(integers)})")

# Create single-dataset dictionary for compatibility with existing code
datasets = {dataset_name: integers}


# Generate corpus seeds for bitmap_api
print("\nGenerating bitmap_api corpus...")
api_dir = os.path.join(corpus_base, "bitmap_api")
os.makedirs(api_dir, exist_ok=True)

seed_count = 0
for dataset_name, integers in datasets.items():
    if not integers:
        continue

    # Small seed (100 integers)
    sample = integers[:min(100, len(integers))]
    # OP_SETINTARRAY (2), then size, then integers
    seed_data = struct.pack("<B", 2)
    seed_data += struct.pack(f"<{len(sample)}I", *sample)

    with open(os.path.join(api_dir, f"seed_{dataset_name}_small"), "wb") as f:
        f.write(seed_data)
    seed_count += 1

    # Medium seed (1000 integers) if available
    if len(integers) >= 1000:
        sample = integers[:1000]
        seed_data = struct.pack("<B", 2)
        seed_data += struct.pack(f"<{len(sample)}I", *sample)

        with open(os.path.join(api_dir, f"seed_{dataset_name}_medium"), "wb") as f:
            f.write(seed_data)
        seed_count += 1

print(f"  Created {seed_count} seeds")

# Generate corpus seeds for bitmap64_api
print("\nGenerating bitmap64_api corpus...")
api64_dir = os.path.join(corpus_base, "bitmap64_api")
os.makedirs(api64_dir, exist_ok=True)

seed_count = 0
for dataset_name, integers in datasets.items():
    if not integers:
        continue

    # Convert to 64-bit and add large offsets for testing 64-bit range
    sample_32 = integers[:min(50, len(integers))]
    sample_64 = [int(x) for x in sample_32]
    # Also add some with high bits set
    sample_64.extend([x + (1 << 32) for x in sample_32[:min(25, len(sample_32))]])

    # OP64_SETINTARRAY (2)
    seed_data = struct.pack("<B", 2)
    seed_data += struct.pack(f"<{len(sample_64)}Q", *sample_64)

    with open(os.path.join(api64_dir, f"seed_{dataset_name}_64bit"), "wb") as f:
        f.write(seed_data)
    seed_count += 1

print(f"  Created {seed_count} seeds")

# Generate corpus seeds for operations
print("\nGenerating operations corpus...")
ops_dir = os.path.join(corpus_base, "operations")
os.makedirs(ops_dir, exist_ok=True)

seed_count = 0
dataset_list = list(datasets.items())

# Create seeds with pairs of datasets for operations
for i, (dataset_name, integers) in enumerate(dataset_list):
    if len(integers) < 50:
        continue

    # Take two portions of the same dataset
    set1 = integers[:min(50, len(integers) // 2)]
    set2 = integers[len(integers)//2:len(integers)//2 + min(50, len(integers) // 2)]

    if not set1 or not set2:
        continue

    # Format: num_inputs (2), operation (0=AND), pattern byte for set1, integers for set1, pattern for set2, integers for set2
    for op_idx, op_name in enumerate(['and', 'or', 'xor']):
        seed_data = struct.pack("<BBB", 2, op_idx, 3)  # 2 inputs, operation, pattern 3 (from array)
        seed_data += struct.pack(f"<{len(set1)}I", *set1)
        seed_data += struct.pack("<B", 3)  # pattern for second set
        seed_data += struct.pack(f"<{len(set2)}I", *set2)

        with open(os.path.join(ops_dir, f"seed_{dataset_name}_{op_name}"), "wb") as f:
            f.write(seed_data)
        seed_count += 1

print(f"  Created {seed_count} seeds")

# Generate corpus seeds for serialization
print("\nGenerating serialization corpus...")
ser_dir = os.path.join(corpus_base, "serialization")
os.makedirs(ser_dir, exist_ok=True)

seed_count = 0
for dataset_name, integers in datasets.items():
    if not integers:
        continue

    # Test int array round-trip
    sample = integers[:min(200, len(integers))]
    seed_data = struct.pack("<B", 0)  # test_type 0 = int array 32-bit
    seed_data += struct.pack(f"<{len(sample)}I", *sample)

    with open(os.path.join(ser_dir, f"seed_{dataset_name}_intarray"), "wb") as f:
        f.write(seed_data)
    seed_count += 1

    # Test int array 64-bit
    sample_64 = [int(x) for x in integers[:min(100, len(integers))]]
    seed_data = struct.pack("<B", 1)  # test_type 1 = int array 64-bit
    seed_data += struct.pack(f"<{len(sample_64)}Q", *sample_64)

    with open(os.path.join(ser_dir, f"seed_{dataset_name}_intarray64"), "wb") as f:
        f.write(seed_data)
    seed_count += 1

    # Test range operations
    if len(integers) >= 2:
        from_val = min(integers[:100])
        to_val = max(integers[:100])
        # Limit range size
        if to_val - from_val > 1000000:
            to_val = from_val + 1000000

        seed_data = struct.pack("<BII", 4, from_val, to_val)  # test_type 4 = range 32-bit
        with open(os.path.join(ser_dir, f"seed_{dataset_name}_range"), "wb") as f:
            f.write(seed_data)
        seed_count += 1

print(f"  Created {seed_count} seeds")

print(f"\n✓ Generated corpus seeds from real data:")
print(f"  Total datasets used: {len(datasets)}")
print(f"  Corpus directories:")
print(f"    - bitmap_api/")
print(f"    - bitmap64_api/")
print(f"    - operations/")
print(f"    - serialization/")

PYTHON_EOF

echo ""
echo "✓ Corpus generation complete"
echo ""
echo "Corpus files:"
find "$CORPUS_DIR" -type f -name "seed_*" | wc -l | xargs echo "  Total seeds:"
