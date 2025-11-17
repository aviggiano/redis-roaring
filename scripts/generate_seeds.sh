#!/usr/bin/env bash
#
# Generate initial seed files for fuzzers
# These are simple byte sequences to help the fuzzer start

set -e

# Create corpus directories
mkdir -p tests/fuzz/corpus/bitmap_api
mkdir -p tests/fuzz/corpus/bitmap64_api
mkdir -p tests/fuzz/corpus/bitmap_operations
mkdir -p tests/fuzz/corpus/bitmap_serialization

echo "Generating seed files for fuzzers..."

# Generate seeds for bitmap_api (32-bit)
# Seed 1: Simple setbit operation (op=0, value=1)
printf '\x00\x00\x00\x00\x01' > tests/fuzz/corpus/bitmap_api/seed1_setbit

# Seed 2: Add some integers (op=1, count=2, values)
printf '\x01\x02\x00\x00\x00\x64\x00\x00\x00\xc8' > tests/fuzz/corpus/bitmap_api/seed2_intarray

# Seed 3: Bitwise operations (op=2, op_type=0)
printf '\x02\x00\x01\x00' > tests/fuzz/corpus/bitmap_api/seed3_bitop

# Seed 4: Range operations
printf '\x03\x00\x00\x00\x00\x00\x00\x03\xe8' > tests/fuzz/corpus/bitmap_api/seed4_range

# Seed 5: Mixed operations
printf '\x00\x01\x00\x00\x02\x03\x00\x00\x00\x64' > tests/fuzz/corpus/bitmap_api/seed5_mixed

# Generate seeds for bitmap64_api (64-bit)
# Seed 1: Simple setbit with 64-bit value
printf '\x00\x00\x00\x00\x00\x00\x00\x00\x01' > tests/fuzz/corpus/bitmap64_api/seed1_setbit64

# Seed 2: Large 64-bit values
printf '\x01\x02\x00\x00\x00\x00\xff\xff\xff\xff\x00\x00\x00\x01\x00\x00\x00\x00' > tests/fuzz/corpus/bitmap64_api/seed2_large64

# Seed 3: Bitwise operations
printf '\x02\x00\x01\x00' > tests/fuzz/corpus/bitmap64_api/seed3_bitop64

# Seed 4: Range with 64-bit
printf '\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x27\x10' > tests/fuzz/corpus/bitmap64_api/seed4_range64

# Generate seeds for bitmap_operations (combined operations)
# Seed 1: OR operation
printf '\x00\x01\x00\x00\x00\x64\x00\x00\x00\xc8' > tests/fuzz/corpus/bitmap_operations/seed1_or

# Seed 2: AND operation
printf '\x01\x02\x00\x00\x00\x32\x00\x00\x00\x64' > tests/fuzz/corpus/bitmap_operations/seed2_and

# Seed 3: XOR operation
printf '\x02\x02\x00\x00\x00\x0a\x00\x00\x00\x14' > tests/fuzz/corpus/bitmap_operations/seed3_xor

# Seed 4: ANDNOT operation
printf '\x03\x01\x00\x00\x00\xff' > tests/fuzz/corpus/bitmap_operations/seed4_andnot

# Generate seeds for bitmap_serialization
# Seed 1: Small serialized bitmap
printf '\x01\x00\x00\x00\x04\x00\x00\x00\x01\x00\x00\x00' > tests/fuzz/corpus/bitmap_serialization/seed1_small

# Seed 2: Multiple values
printf '\x02\x00\x00\x00\x08\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00' > tests/fuzz/corpus/bitmap_serialization/seed2_multi

# Seed 3: Range pattern
printf '\x03\x00\x00\x00\x10' > tests/fuzz/corpus/bitmap_serialization/seed3_range

# Seed 4: Empty
printf '\x00' > tests/fuzz/corpus/bitmap_serialization/seed4_empty

echo "✓ Seed files generated successfully!"
echo "  - bitmap_api: 5 seeds"
echo "  - bitmap64_api: 4 seeds"
echo "  - bitmap_operations: 4 seeds"
echo "  - bitmap_serialization: 4 seeds"
