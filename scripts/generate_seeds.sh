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
mkdir -p tests/fuzz/corpus/bitop_keys
mkdir -p tests/fuzz/corpus/command_metadata
mkdir -p tests/fuzz/corpus/command_dispatch
mkdir -p tests/fuzz/corpus/cluster_routing
mkdir -p tests/fuzz/corpus/persistence_sequences
mkdir -p tests/fuzz/corpus/r_vs_r64_parity

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

# Generate seeds for bitop_keys
# Seed 1: Variadic OR with minimum valid arity
printf '\x01\x05\x00' > tests/fuzz/corpus/bitop_keys/seed1_or_min

# Seed 2: Unary NOT with explicit size argument
printf '\x03\x05\x00' > tests/fuzz/corpus/bitop_keys/seed2_not_with_size

# Seed 3: Unsupported operation should yield no keys
printf '\x08\x06\x00' > tests/fuzz/corpus/bitop_keys/seed3_invalid_op

# Seed 4: Variadic op with invalid arity
printf '\x00\x04\x00' > tests/fuzz/corpus/bitop_keys/seed4_invalid_arity

# Generate seeds for command_metadata
# Seed 1: single-key command family with valid arity
printf '\x00\x00\x00\x00' > tests/fuzz/corpus/command_metadata/seed1_single_key

# Seed 2: pair-key command family with optional mode
printf '\x16\x01\x01\x01' > tests/fuzz/corpus/command_metadata/seed2_pair_key

# Seed 3: BITOP metadata path
printf '\x0f\x01\x00\x00\x00\x01' > tests/fuzz/corpus/command_metadata/seed3_bitop

# Generate seeds for command_dispatch
# Seed 1: 32-bit OR with distinct sources
printf '\x00\x01\x00\x00\x01\x02\x03\x04' > tests/fuzz/corpus/command_dispatch/seed1_or32

# Seed 2: 64-bit NOT with explicit size
printf '\x01\x03\x00\x01\x08\x00\x00\x00' > tests/fuzz/corpus/command_dispatch/seed2_not64

# Seed 3: invalid BITOP operation
printf '\x00\x08\x00\x00\x01' > tests/fuzz/corpus/command_dispatch/seed3_invalid

# Generate seeds for cluster_routing
# Seed 1: same-slot OR via direct command path
printf '\x00\x00\x00\x00\x00\x01\x02\x03' > tests/fuzz/corpus/cluster_routing/seed1_same_slot

# Seed 2: cross-slot NOT via direct command path
printf '\x01\x01\x01\x00\x01\x01\x02\x03' > tests/fuzz/corpus/cluster_routing/seed2_cross_slot

# Seed 3: same-slot OR via FCALL function-routing path
printf '\x00\x00\x00\x01\x00\x01\x02\x03' > tests/fuzz/corpus/cluster_routing/seed3_function_route

# Generate seeds for persistence_sequences
# Seed 1: short 32-bit sequence
printf '\x00\x01\x02\x03\x04\x05\x06\x07' > tests/fuzz/corpus/persistence_sequences/seed1_rdb_aof_32

# Seed 2: short 64-bit sequence
printf '\x01\x02\x03\x04\x05\x06\x07\x08' > tests/fuzz/corpus/persistence_sequences/seed2_rdb_aof_64

# Generate seeds for r_vs_r64_parity
# Seed 1: SETBIT parity
printf '\x00\x10\x01' > tests/fuzz/corpus/r_vs_r64_parity/seed1_setbit

# Seed 2: GETBITS parity with multiple offsets
printf '\x02\x03\x20\x40\x60' > tests/fuzz/corpus/r_vs_r64_parity/seed2_getbits

# Seed 3: RANGEINTARRAY parity
printf '\x06\x02\x04' > tests/fuzz/corpus/r_vs_r64_parity/seed3_range

# Seed 4: DIFF parity with aliasing pressure
printf '\x09\x02\x01\x02\x03\x04' > tests/fuzz/corpus/r_vs_r64_parity/seed4_diff_alias

# Seed 5: SETFULL shared-range allowlist
printf '\x0a' > tests/fuzz/corpus/r_vs_r64_parity/seed5_setfull

# Seed 6: SETBITARRAY parity
printf '\x0d\x01\x01\x01\x01' > tests/fuzz/corpus/r_vs_r64_parity/seed6_setbitarray

# Seed 7: BITOP parity with unary-style aliasing
printf '\x0f\x03\x01\x01\x02\x03\x04' > tests/fuzz/corpus/r_vs_r64_parity/seed7_bitop_alias

# Seed 8: CONTAINS parity with explicit mode
printf '\x15\x01\x01\x02' > tests/fuzz/corpus/r_vs_r64_parity/seed8_contains

# Seed 9: JACCARD parity with same-key comparison
printf '\x16\x01\x00' > tests/fuzz/corpus/r_vs_r64_parity/seed9_jaccard

echo "✓ Seed files generated successfully!"
echo "  - bitmap_api: 5 seeds"
echo "  - bitmap64_api: 4 seeds"
echo "  - bitmap_operations: 4 seeds"
echo "  - bitmap_serialization: 4 seeds"
echo "  - bitop_keys: 4 seeds"
echo "  - command_metadata: 3 seeds"
echo "  - command_dispatch: 3 seeds"
echo "  - cluster_routing: 3 seeds"
echo "  - persistence_sequences: 2 seeds"
echo "  - r_vs_r64_parity: 9 seeds"
