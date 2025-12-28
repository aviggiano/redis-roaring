#ifndef FUZZ_COMMON_H
#define FUZZ_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "../../src/data-structure.h"

/* Maximum number of bitmaps to maintain in fuzzer state */
#define MAX_FUZZ_BITMAPS 5

/* Maximum operations per fuzzing iteration */
#define MAX_FUZZ_OPERATIONS 1000

/* Maximum array size for int/bit arrays */
#define MAX_ARRAY_SIZE 10000

/* Maximum bit offset to prevent excessive memory usage */
#define MAX_BIT_OFFSET_32 10000000
#define MAX_BIT_OFFSET_64 100000000

/* Maximum cardinality for expensive operations */
#define MAX_SAFE_CARDINALITY 10000000

/* Bitmap operation types for 32-bit API */
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

/* Bitmap operation types for 64-bit API */
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

/* Simple input parser for fuzzing */
typedef struct {
    const uint8_t* data;
    size_t size;
    size_t offset;
} FuzzInput;

/* Initialize fuzzer input */
static inline void fuzz_input_init(FuzzInput* input, const uint8_t* data, size_t size) {
    input->data = data;
    input->size = size;
    input->offset = 0;
}

/* Get remaining bytes */
static inline size_t fuzz_input_remaining(const FuzzInput* input) {
    return input->size - input->offset;
}

/* Consume a single byte */
static inline uint8_t fuzz_consume_u8(FuzzInput* input) {
    if (input->offset >= input->size) {
        return 0;
    }
    return input->data[input->offset++];
}

/* Consume a uint16_t */
static inline uint16_t fuzz_consume_u16(FuzzInput* input) {
    uint16_t result = 0;
    for (int i = 0; i < 2 && input->offset < input->size; i++) {
        result |= ((uint16_t)input->data[input->offset++]) << (i * 8);
    }
    return result;
}

/* Consume a uint32_t */
static inline uint32_t fuzz_consume_u32(FuzzInput* input) {
    uint32_t result = 0;
    for (int i = 0; i < 4 && input->offset < input->size; i++) {
        result |= ((uint32_t)input->data[input->offset++]) << (i * 8);
    }
    return result;
}

/* Consume a uint64_t */
static inline uint64_t fuzz_consume_u64(FuzzInput* input) {
    uint64_t result = 0;
    for (int i = 0; i < 8 && input->offset < input->size; i++) {
        result |= ((uint64_t)input->data[input->offset++]) << (i * 8);
    }
    return result;
}

/* Consume a bool */
static inline bool fuzz_consume_bool(FuzzInput* input) {
    return (fuzz_consume_u8(input) & 1) != 0;
}

/* Consume an integer in range [min, max] */
static inline uint32_t fuzz_consume_u32_in_range(FuzzInput* input, uint32_t min, uint32_t max) {
    if (min >= max) return min;
    uint32_t range = max - min + 1;
    uint32_t value = fuzz_consume_u32(input);
    return min + (value % range);
}

/* Consume an integer in range [min, max] for uint64_t */
static inline uint64_t fuzz_consume_u64_in_range(FuzzInput* input, uint64_t min, uint64_t max) {
    if (min >= max) return min;
    uint64_t range = max - min + 1;
    uint64_t value = fuzz_consume_u64(input);
    return min + (value % range);
}

/* Consume a size_t in range [min, max] */
static inline size_t fuzz_consume_size_in_range(FuzzInput* input, size_t min, size_t max) {
    if (min >= max) return min;
    size_t range = max - min + 1;
    size_t value = (size_t)fuzz_consume_u32(input);
    return min + (value % range);
}

/* Helper function to safely free bitmap arrays */
static inline void safe_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

static inline void fuzz_require(bool condition) {
    if (!condition) {
        abort();
    }
}

static inline size_t fuzz_expected_range_count_u64(uint64_t card, size_t start, size_t end) {
    if (end < start) return 0;
    if (card <= start) return 0;

    size_t requested = end - start + 1;
    size_t available = (size_t)(card - start);
    return (available < requested) ? available : requested;
}

static inline void fuzz_check_range_int_array32(const Bitmap* bitmap, size_t start, size_t end,
                                                const uint32_t* result, size_t result_count) {
    if (!result) return;

    uint64_t card = bitmap_get_cardinality(bitmap);
    size_t expected_count = fuzz_expected_range_count_u64(card, start, end);
    fuzz_require(result_count == expected_count);

    for (size_t i = 0; i < result_count; i++) {
        int64_t expected = bitmap_get_nth_element_present(bitmap, start + i + 1);
        fuzz_require(expected >= 0);
        fuzz_require(result[i] == (uint32_t)expected);
    }
}

static inline uint64_t fuzz_expected_range_count_u64u64(uint64_t card, uint64_t start, uint64_t end) {
    if (end < start) return 0;
    if (card <= start) return 0;

    uint64_t requested = end - start + 1;
    uint64_t available = card - start;
    return (available < requested) ? available : requested;
}

static inline void fuzz_check_range_int_array64(const Bitmap64* bitmap, uint64_t start, uint64_t end,
                                                const uint64_t* result, uint64_t result_count) {
    if (!result) return;

    uint64_t card = bitmap64_get_cardinality(bitmap);
    uint64_t expected_count = fuzz_expected_range_count_u64u64(card, start, end);
    fuzz_require(result_count == expected_count);

    for (uint64_t i = 0; i < result_count; i++) {
        bool found = false;
        uint64_t expected = bitmap64_get_nth_element_present(bitmap, start + i + 1, &found);
        fuzz_require(found);
        fuzz_require(result[i] == expected);
    }
}

/* Helper to generate a random uint32 array */
static inline uint32_t* generate_uint32_array(FuzzInput* input, size_t* out_size) {
    size_t size = fuzz_consume_size_in_range(input, 0, MAX_ARRAY_SIZE);
    if (size == 0) {
        *out_size = 0;
        return NULL;
    }

    uint32_t* array = (uint32_t*)malloc(sizeof(uint32_t) * size);
    if (!array) {
        *out_size = 0;
        return NULL;
    }

    for (size_t i = 0; i < size; i++) {
        array[i] = fuzz_consume_u32_in_range(input, 0, MAX_BIT_OFFSET_32);
    }

    *out_size = size;
    return array;
}

/* Helper to generate a random uint64 array */
static inline uint64_t* generate_uint64_array(FuzzInput* input, size_t* out_size) {
    size_t size = fuzz_consume_size_in_range(input, 0, MAX_ARRAY_SIZE);
    if (size == 0) {
        *out_size = 0;
        return NULL;
    }

    uint64_t* array = (uint64_t*)malloc(sizeof(uint64_t) * size);
    if (!array) {
        *out_size = 0;
        return NULL;
    }

    for (size_t i = 0; i < size; i++) {
        array[i] = fuzz_consume_u64_in_range(input, 0, MAX_BIT_OFFSET_64);
    }

    *out_size = size;
    return array;
}

/* Helper to generate a random bit array string */
static inline char* generate_bit_array_string(FuzzInput* input, size_t* out_size) {
    size_t size = fuzz_consume_size_in_range(input, 0, MAX_ARRAY_SIZE);
    if (size == 0) {
        *out_size = 0;
        return NULL;
    }

    char* array = (char*)malloc(size + 1);
    if (!array) {
        *out_size = 0;
        return NULL;
    }

    for (size_t i = 0; i < size; i++) {
        /* Generate '0' or '1' characters */
        array[i] = fuzz_consume_bool(input) ? '1' : '0';
    }
    array[size] = '\0';

    *out_size = size;
    return array;
}

/* Helper to select a random valid bitmap index */
static inline int select_bitmap_index(FuzzInput* input, int num_bitmaps) {
    if (num_bitmaps <= 0) return 0;
    return (int)fuzz_consume_u32_in_range(input, 0, num_bitmaps - 1);
}

#endif /* FUZZ_COMMON_H */
