#ifndef FUZZ_COMMON_H
#define FUZZ_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// FuzzedDataProvider for C++ fuzz targets
#include <fuzzer/FuzzedDataProvider.h>

// Include the data structure header
// Note: roaring.h needs to be included outside extern "C" since it may include C++ headers
#include "../../src/data-structure.h"

// Maximum number of bitmaps to maintain in fuzzer state
#define MAX_FUZZ_BITMAPS 5

// Maximum operations per fuzzing iteration
#define MAX_FUZZ_OPERATIONS 1000

// Maximum array size for int/bit arrays
#define MAX_ARRAY_SIZE 10000

// Bitmap operation types for 32-bit API
enum BitmapOperation {
    OP_SETBIT = 0,
    OP_GETBIT,
    OP_SETINTARRAY,
    OP_GETINTARRAY,
    OP_APPENDINTARRAY,
    OP_RANGEINTARRAY,
    OP_SETBITARRAY,
    OP_GETBITARRAY,
    OP_SETRANGE,
    OP_BITOP_AND,
    OP_BITOP_OR,
    OP_BITOP_XOR,
    OP_BITOP_NOT,
    OP_BITOP_ANDOR,
    OP_BITOP_ANDNOT,
    OP_BITOP_ORNOT,
    OP_BITOP_ONE,
    OP_MIN,
    OP_MAX,
    OP_OPTIMIZE,
    OP_STATISTICS,
    OP_CARDINALITY,
    OP_ISEMPTY,
    OP_GETBITS,
    OP_CLEARBITS,
    OP_CLEARBITS_COUNT,
    OP_INTERSECT,
    OP_JACCARD,
    OP_GET_NTH_ELEMENT,
    OP_FREE_AND_ALLOC,
    NUM_BITMAP_OPERATIONS
};

// Bitmap operation types for 64-bit API
enum Bitmap64Operation {
    OP64_SETBIT = 0,
    OP64_GETBIT,
    OP64_SETINTARRAY,
    OP64_GETINTARRAY,
    OP64_APPENDINTARRAY,
    OP64_RANGEINTARRAY,
    OP64_SETBITARRAY,
    OP64_GETBITARRAY,
    OP64_SETRANGE,
    OP64_BITOP_AND,
    OP64_BITOP_OR,
    OP64_BITOP_XOR,
    OP64_BITOP_NOT,
    OP64_BITOP_ANDOR,
    OP64_BITOP_ANDNOT,
    OP64_BITOP_ORNOT,
    OP64_BITOP_ONE,
    OP64_MIN,
    OP64_MAX,
    OP64_OPTIMIZE,
    OP64_STATISTICS,
    OP64_CARDINALITY,
    OP64_ISEMPTY,
    OP64_GETBITS,
    OP64_CLEARBITS,
    OP64_CLEARBITS_COUNT,
    OP64_INTERSECT,
    OP64_JACCARD,
    OP64_GET_NTH_ELEMENT,
    OP64_FREE_AND_ALLOC,
    NUM_BITMAP64_OPERATIONS
};

// Helper function to safely free bitmap arrays
static inline void safe_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

// Helper to generate a random uint32 array
static inline uint32_t* generate_uint32_array(FuzzedDataProvider& fdp, size_t* out_size) {
    size_t size = fdp.ConsumeIntegralInRange<size_t>(0, MAX_ARRAY_SIZE);
    if (size == 0) {
        *out_size = 0;
        return nullptr;
    }

    uint32_t* array = (uint32_t*)malloc(sizeof(uint32_t) * size);
    if (!array) {
        *out_size = 0;
        return nullptr;
    }

    for (size_t i = 0; i < size; i++) {
        array[i] = fdp.ConsumeIntegral<uint32_t>();
    }

    *out_size = size;
    return array;
}

// Helper to generate a random uint64 array
static inline uint64_t* generate_uint64_array(FuzzedDataProvider& fdp, size_t* out_size) {
    size_t size = fdp.ConsumeIntegralInRange<size_t>(0, MAX_ARRAY_SIZE);
    if (size == 0) {
        *out_size = 0;
        return nullptr;
    }

    uint64_t* array = (uint64_t*)malloc(sizeof(uint64_t) * size);
    if (!array) {
        *out_size = 0;
        return nullptr;
    }

    for (size_t i = 0; i < size; i++) {
        array[i] = fdp.ConsumeIntegral<uint64_t>();
    }

    *out_size = size;
    return array;
}

// Helper to generate a random bit array string
static inline char* generate_bit_array_string(FuzzedDataProvider& fdp, size_t* out_size) {
    size_t size = fdp.ConsumeIntegralInRange<size_t>(0, MAX_ARRAY_SIZE);
    if (size == 0) {
        *out_size = 0;
        return nullptr;
    }

    char* array = (char*)malloc(size + 1);
    if (!array) {
        *out_size = 0;
        return nullptr;
    }

    for (size_t i = 0; i < size; i++) {
        // Generate '0' or '1' characters
        array[i] = fdp.ConsumeBool() ? '1' : '0';
    }
    array[size] = '\0';

    *out_size = size;
    return array;
}

// Helper to select a random valid bitmap index
static inline int select_bitmap_index(FuzzedDataProvider& fdp, int num_bitmaps) {
    if (num_bitmaps <= 0) return 0;
    return fdp.ConsumeIntegralInRange<int>(0, num_bitmaps - 1);
}

#endif // FUZZ_COMMON_H
