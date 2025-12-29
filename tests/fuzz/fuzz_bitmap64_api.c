/*
 * Fuzzer for 64-bit Bitmap API (Simplified)
 *
 * This fuzzer tests the main redis-roaring bitmap operations with:
 * - Single bitmap per iteration (no state management)
 * - Random sequence of operations
 * - Edge cases and error handling
 */

#include "fuzz_common.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    /* Need at least a few bytes to do anything interesting */
    if (size < 4) {
        return 0;
    }

    FuzzInput input;
    fuzz_input_init(&input, data, size);

    /* Single bitmap - no state management */
    Bitmap64* bitmap = bitmap64_alloc();
    if (!bitmap) {
        return 0;
    }

    /* Perform random operations */
    int operations = 0;
    while (fuzz_input_remaining(&input) > 0 && operations < MAX_FUZZ_OPERATIONS) {
        operations++;

        uint8_t op = fuzz_consume_u8(&input) % NUM_BITMAP64_OPERATIONS;

        switch (op) {
            case OP_SETBIT: {
                uint64_t offset = fuzz_consume_bool(&input)
                    ? 0
                    : fuzz_consume_u64_in_range(&input, 0, MAX_BIT_OFFSET_64);
                bool value = fuzz_consume_bool(&input);
                bool prev = bitmap64_getbit(bitmap, offset);
                bool ret = bitmap64_setbit(bitmap, offset, value);
                fuzz_require(ret == prev);
                fuzz_require(bitmap64_getbit(bitmap, offset) == value);
                break;
            }

            case OP_GETBIT: {
                uint64_t offset = fuzz_consume_u64_in_range(&input, 0, MAX_BIT_OFFSET_64);
                bitmap64_getbit(bitmap, offset);
                break;
            }

            case OP_SETINTARRAY: {
                size_t array_size;
                uint64_t* array = generate_uint64_array(&input, &array_size);
                if (array && array_size > 0) {
                    if (fuzz_consume_bool(&input)) {
                        array[0] = 0;
                        if (array_size > 1) array[1] = 8;
                        if (array_size > 2) array[2] = 16;
                    }
                    Bitmap64* new_bitmap = bitmap64_from_int_array(array_size, array);
                    if (new_bitmap) {
                        bitmap64_free(bitmap);
                        bitmap = new_bitmap;
                    }
                    safe_free(array);
                }
                break;
            }

            case OP_GETINTARRAY: {
                /* Only get array if cardinality is reasonable */
                uint64_t card = bitmap64_get_cardinality(bitmap);
                if (card > 0 && card < MAX_SAFE_CARDINALITY) {
                    uint64_t result_size;
                    uint64_t* result = bitmap64_get_int_array(bitmap, &result_size);
                    if (card <= MAX_RANGE_VALIDATE_CARDINALITY) {
                        fuzz_check_int_array64(bitmap, result, result_size);
                    }
                    safe_free(result);
                }
                break;
            }

            case OP_APPENDINTARRAY: {
                size_t array_size;
                uint64_t* array = generate_uint64_array(&input, &array_size);
                if (array && array_size > 0) {
                    if (fuzz_consume_bool(&input)) {
                        array[0] = 0;
                        if (array_size > 1) array[1] = 8;
                        if (array_size > 2) array[2] = 16;
                    }
                    Bitmap64* new_values = bitmap64_from_int_array(array_size, array);
                    if (new_values) {
                        roaring64_bitmap_or_inplace(bitmap, new_values);
                        bitmap64_free(new_values);
                    }
                    safe_free(array);
                }
                break;
            }

            case OP_RANGEINTARRAY: {
                uint64_t card = bitmap64_get_cardinality(bitmap);
                if (card > 0) {
                    uint64_t start = 0;
                    uint64_t end = 0;
                    if ((card >= 3) && fuzz_consume_bool(&input)) {
                        start = 0;
                        end = 2;
                    } else {
                        start = fuzz_consume_u64_in_range(&input, 0, card);
                        end = fuzz_consume_u64_in_range(&input, start, card + 100);
                    }
                    uint64_t result_count;
                    uint64_t* result = bitmap64_range_int_array(bitmap, start, end, &result_count);
                    fuzz_check_range_int_array64(bitmap, start, end, result, result_count);
                    safe_free(result);
                }
                break;
            }

            case OP_SETBITARRAY: {
                size_t bit_array_size;
                char* bit_array = generate_bit_array_string(&input, &bit_array_size);
                if (bit_array && bit_array_size > 0) {
                    Bitmap64* new_bitmap = bitmap64_from_bit_array(bit_array_size, bit_array);
                    if (new_bitmap) {
                        if (bit_array_size <= MAX_BITARRAY_VALIDATE_SIZE) {
                            fuzz_check_input_bit_array64(new_bitmap, bit_array, bit_array_size);
                        }
                        bitmap64_free(bitmap);
                        bitmap = new_bitmap;
                    }
                    safe_free(bit_array);
                }
                break;
            }

            case OP_GETBITARRAY: {
                /* Only get bit array if max value is reasonable */
                if (!bitmap64_is_empty(bitmap)) {
                    uint64_t max_val = bitmap64_max(bitmap);
                    if (max_val < MAX_BIT_OFFSET_64) {
                        size_t result_size;
                        char* bit_array = bitmap64_get_bit_array(bitmap, &result_size);
                        if (result_size <= MAX_BITARRAY_VALIDATE_SIZE) {
                            fuzz_check_bit_array64(bitmap, bit_array, result_size);
                        }
                        bitmap_free_bit_array(bit_array);
                    }
                }
                break;
            }

            case OP_SETRANGE: {
                /* Clamp both from and range size to avoid excessive memory */
                uint64_t from = fuzz_consume_u64_in_range(&input, 0, MAX_BIT_OFFSET_64);
                uint64_t range_size = fuzz_consume_u64_in_range(&input, 0, 1000000);
                uint64_t to = from + range_size;
                Bitmap64* range_bitmap = bitmap64_from_range(from, to);
                if (range_bitmap) {
                    bitmap64_free(bitmap);
                    bitmap = range_bitmap;
                }
                break;
            }

            case OP_BITOP_AND:
            case OP_BITOP_OR:
            case OP_BITOP_XOR:
            case OP_BITOP_ANDOR:
            case OP_BITOP_ANDNOT:
            case OP_BITOP_ORNOT:
            case OP_BITOP_ONE: {
                /* Create temporary second bitmap for binary operations */
                Bitmap64* bitmap2 = bitmap64_alloc();
                if (bitmap2) {
                    /* Add some random bits to second bitmap */
                    for (int i = 0; i < 10 && fuzz_input_remaining(&input) > sizeof(uint64_t); i++) {
                        uint64_t offset = fuzz_consume_u64_in_range(&input, 0, MAX_BIT_OFFSET_64);
                        bitmap64_setbit(bitmap2, offset, true);
                    }

                    const Bitmap64* inputs[2] = {bitmap, bitmap2};
                    Bitmap64* result = bitmap64_alloc();
                    if (result) {
                        switch (op) {
                            case OP_BITOP_AND:
                                bitmap64_and(result, 2, inputs);
                                break;
                            case OP_BITOP_OR:
                                bitmap64_or(result, 2, inputs);
                                break;
                            case OP_BITOP_XOR:
                                bitmap64_xor(result, 2, inputs);
                                break;
                            case OP_BITOP_ANDOR:
                                bitmap64_andor(result, 2, inputs);
                                break;
                            case OP_BITOP_ANDNOT:
                                bitmap64_andnot(result, 2, inputs);
                                break;
                            case OP_BITOP_ORNOT:
                                bitmap64_ornot(result, 2, inputs);
                                break;
                            case OP_BITOP_ONE:
                                bitmap64_one(result, 2, inputs);
                                break;
                        }
                        bitmap64_free(result);
                    }
                    bitmap64_free(bitmap2);
                }
                break;
            }

            case OP_BITOP_NOT: {
                Bitmap64* result = bitmap64_not(bitmap);
                if (result) {
                    bitmap64_free(result);
                }
                break;
            }

            case OP_MIN: {
                if (!bitmap64_is_empty(bitmap)) {
                    uint64_t min = bitmap64_min(bitmap);
                    uint64_t card = bitmap64_get_cardinality(bitmap);
                    if (card > 0 && card <= MAX_RANGE_VALIDATE_CARDINALITY) {
                        bool found = false;
                        uint64_t expected = bitmap64_get_nth_element_present(bitmap, 1, &found);
                        fuzz_require(found);
                        fuzz_require(min == expected);
                    }
                }
                break;
            }

            case OP_MAX: {
                if (!bitmap64_is_empty(bitmap)) {
                    uint64_t max = bitmap64_max(bitmap);
                    uint64_t card = bitmap64_get_cardinality(bitmap);
                    if (card > 0 && card <= MAX_RANGE_VALIDATE_CARDINALITY) {
                        bool found = false;
                        uint64_t expected = bitmap64_get_nth_element_present(bitmap, card, &found);
                        fuzz_require(found);
                        fuzz_require(max == expected);
                    }
                }
                break;
            }

            case OP_OPTIMIZE: {
                bool shrink = fuzz_consume_bool(&input);
                bitmap64_optimize(bitmap, shrink);
                break;
            }

            case OP_STATISTICS: {
                int format = fuzz_consume_u8(&input) % 2;
                int size_out;
                char* stats = bitmap64_statistics_str(bitmap, format, &size_out);
                safe_free(stats);
                break;
            }

            case OP_CARDINALITY: {
                bitmap64_get_cardinality(bitmap);
                break;
            }

            case OP_ISEMPTY: {
                bitmap64_is_empty(bitmap);
                break;
            }

            case OP_GETBITS: {
                size_t n_offsets = fuzz_consume_size_in_range(&input, 1, 100);
                uint64_t* offsets = (uint64_t*)malloc(n_offsets * sizeof(uint64_t));
                if (offsets) {
                    for (size_t i = 0; i < n_offsets && fuzz_input_remaining(&input) > sizeof(uint64_t); i++) {
                        offsets[i] = fuzz_consume_u64_in_range(&input, 0, MAX_BIT_OFFSET_64);
                    }
                    bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
                    if (results) {
                        for (size_t i = 0; i < n_offsets; i++) {
                            fuzz_require(results[i] == bitmap64_getbit(bitmap, offsets[i]));
                        }
                    }
                    safe_free(results);
                    safe_free(offsets);
                }
                break;
            }

            case OP_CLEARBITS: {
                size_t n_offsets = fuzz_consume_size_in_range(&input, 1, 100);
                uint64_t* offsets = (uint64_t*)malloc(n_offsets * sizeof(uint64_t));
                if (offsets) {
                    for (size_t i = 0; i < n_offsets && fuzz_input_remaining(&input) > sizeof(uint64_t); i++) {
                        offsets[i] = fuzz_consume_u64_in_range(&input, 0, MAX_BIT_OFFSET_64);
                    }
                    bitmap64_clearbits(bitmap, n_offsets, offsets);
                    for (size_t i = 0; i < n_offsets; i++) {
                        fuzz_require(!bitmap64_getbit(bitmap, offsets[i]));
                    }
                    safe_free(offsets);
                }
                break;
            }

            case OP_CLEARBITS_COUNT: {
                size_t n_offsets = fuzz_consume_size_in_range(&input, 1, 100);
                uint64_t* offsets = (uint64_t*)malloc(n_offsets * sizeof(uint64_t));
                if (offsets) {
                    for (size_t i = 0; i < n_offsets && fuzz_input_remaining(&input) > sizeof(uint64_t); i++) {
                        offsets[i] = fuzz_consume_u64_in_range(&input, 0, MAX_BIT_OFFSET_64);
                    }
                    uint64_t pre_card = bitmap64_get_cardinality(bitmap);
                    size_t removed = bitmap64_clearbits_count(bitmap, n_offsets, offsets);
                    uint64_t post_card = bitmap64_get_cardinality(bitmap);
                    fuzz_require(pre_card >= post_card);
                    fuzz_require(pre_card - post_card == removed);
                    safe_free(offsets);
                }
                break;
            }

            case OP_INTERSECT: {
                /* Create temporary second bitmap */
                Bitmap64* bitmap2 = bitmap64_alloc();
                if (bitmap2) {
                    for (int i = 0; i < 10 && fuzz_input_remaining(&input) > sizeof(uint64_t); i++) {
                        uint64_t offset = fuzz_consume_u64_in_range(&input, 0, MAX_BIT_OFFSET_64);
                        bitmap64_setbit(bitmap2, offset, true);
                    }
                    uint64_t mode = fuzz_consume_u8(&input) % 4;
                    bitmap64_intersect(bitmap, bitmap2, mode);
                    bitmap64_free(bitmap2);
                }
                break;
            }

            case OP_JACCARD: {
                /* Create temporary second bitmap */
                Bitmap64* bitmap2 = bitmap64_alloc();
                if (bitmap2) {
                    for (int i = 0; i < 10 && fuzz_input_remaining(&input) > sizeof(uint64_t); i++) {
                        uint64_t offset = fuzz_consume_u64_in_range(&input, 0, MAX_BIT_OFFSET_64);
                        bitmap64_setbit(bitmap2, offset, true);
                    }
                    double jaccard = bitmap64_jaccard(bitmap, bitmap2);
                    if (bitmap64_is_empty(bitmap) && bitmap64_is_empty(bitmap2)) {
                        fuzz_require(jaccard == -1);
                    } else {
                        fuzz_require(jaccard >= 0.0 && jaccard <= 1.0);
                    }
                    bitmap64_free(bitmap2);
                }
                break;
            }

            case OP_GET_NTH_ELEMENT: {
                uint64_t n = fuzz_consume_u64(&input);
                if (n < 1000000) {
                    bool found;
                    bitmap64_get_nth_element_present(bitmap, n, &found);
                    bitmap64_get_nth_element_not_present(bitmap, n, &found);
                }
                break;
            }

            case OP_FREE_AND_ALLOC: {
                /* Stress test allocation/deallocation */
                Bitmap64* temp = bitmap64_alloc();
                if (temp) {
                    bitmap64_free(temp);
                }
                break;
            }

            default:
                break;
        }
    }

    bitmap64_free(bitmap);
    return 0;
}
