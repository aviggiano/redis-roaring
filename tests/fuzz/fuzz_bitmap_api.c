/*
 * Fuzzer for 32-bit Bitmap API (Simplified)
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
    Bitmap* bitmap = bitmap_alloc();
    if (!bitmap) {
        return 0;
    }

    /* Perform random operations */
    int operations = 0;
    while (fuzz_input_remaining(&input) > 0 && operations < MAX_FUZZ_OPERATIONS) {
        operations++;

        uint8_t op = fuzz_consume_u8(&input) % NUM_BITMAP_OPERATIONS;

        switch (op) {
            case OP_SETBIT: {
                uint32_t offset = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                bool value = fuzz_consume_bool(&input);
                bitmap_setbit(bitmap, offset, value);
                break;
            }

            case OP_GETBIT: {
                uint32_t offset = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                bitmap_getbit(bitmap, offset);
                break;
            }

            case OP_SETINTARRAY: {
                size_t array_size;
                uint32_t* array = generate_uint32_array(&input, &array_size);
                if (array && array_size > 0) {
                    Bitmap* new_bitmap = bitmap_from_int_array(array_size, array);
                    if (new_bitmap) {
                        bitmap_free(bitmap);
                        bitmap = new_bitmap;
                    }
                    safe_free(array);
                }
                break;
            }

            case OP_GETINTARRAY: {
                /* Only get array if cardinality is reasonable */
                uint64_t card = bitmap_get_cardinality(bitmap);
                if (card > 0 && card < MAX_SAFE_CARDINALITY) {
                    size_t result_size;
                    uint32_t* result = bitmap_get_int_array(bitmap, &result_size);
                    safe_free(result);
                }
                break;
            }

            case OP_APPENDINTARRAY: {
                size_t array_size;
                uint32_t* array = generate_uint32_array(&input, &array_size);
                if (array && array_size > 0) {
                    Bitmap* new_values = bitmap_from_int_array(array_size, array);
                    if (new_values) {
                        roaring_bitmap_or_inplace(bitmap, new_values);
                        bitmap_free(new_values);
                    }
                    safe_free(array);
                }
                break;
            }

            case OP_RANGEINTARRAY: {
                uint64_t card = bitmap_get_cardinality(bitmap);
                if (card > 0) {
                    size_t start = fuzz_consume_size_in_range(&input, 0, card);
                    size_t end = fuzz_consume_size_in_range(&input, start, card + 100);
                    size_t result_count;
                    uint32_t* result = bitmap_range_int_array(bitmap, start, end, &result_count);
                    safe_free(result);
                }
                break;
            }

            case OP_SETBITARRAY: {
                size_t bit_array_size;
                char* bit_array = generate_bit_array_string(&input, &bit_array_size);
                if (bit_array && bit_array_size > 0) {
                    Bitmap* new_bitmap = bitmap_from_bit_array(bit_array_size, bit_array);
                    if (new_bitmap) {
                        bitmap_free(bitmap);
                        bitmap = new_bitmap;
                    }
                    safe_free(bit_array);
                }
                break;
            }

            case OP_GETBITARRAY: {
                /* Only get bit array if max value is reasonable */
                if (!bitmap_is_empty(bitmap)) {
                    uint32_t max_val = bitmap_max(bitmap);
                    if (max_val < MAX_BIT_OFFSET_32) {
                        size_t result_size;
                        char* bit_array = bitmap_get_bit_array(bitmap, &result_size);
                        bitmap_free_bit_array(bit_array);
                    }
                }
                break;
            }

            case OP_SETRANGE: {
                /* Clamp both from and range size to avoid excessive memory */
                uint64_t from = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                uint64_t range_size = fuzz_consume_u32_in_range(&input, 0, 1000000);
                uint64_t to = from + range_size;
                Bitmap* range_bitmap = bitmap_from_range(from, to);
                if (range_bitmap) {
                    bitmap_free(bitmap);
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
                Bitmap* bitmap2 = bitmap_alloc();
                if (bitmap2) {
                    /* Add some random bits to second bitmap */
                    for (int i = 0; i < 10 && fuzz_input_remaining(&input) > sizeof(uint32_t); i++) {
                        uint32_t offset = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                        bitmap_setbit(bitmap2, offset, true);
                    }

                    const Bitmap* inputs[2] = {bitmap, bitmap2};
                    Bitmap* result = bitmap_alloc();
                    if (result) {
                        switch (op) {
                            case OP_BITOP_AND:
                                bitmap_and(result, 2, inputs);
                                break;
                            case OP_BITOP_OR:
                                bitmap_or(result, 2, inputs);
                                break;
                            case OP_BITOP_XOR:
                                bitmap_xor(result, 2, inputs);
                                break;
                            case OP_BITOP_ANDOR:
                                bitmap_andor(result, 2, inputs);
                                break;
                            case OP_BITOP_ANDNOT:
                                bitmap_andnot(result, 2, inputs);
                                break;
                            case OP_BITOP_ORNOT:
                                bitmap_ornot(result, 2, inputs);
                                break;
                            case OP_BITOP_ONE:
                                bitmap_one(result, 2, inputs);
                                break;
                        }
                        bitmap_free(result);
                    }
                    bitmap_free(bitmap2);
                }
                break;
            }

            case OP_BITOP_NOT: {
                Bitmap* result = bitmap_not(bitmap);
                if (result) {
                    bitmap_free(result);
                }
                break;
            }

            case OP_MIN: {
                if (!bitmap_is_empty(bitmap)) {
                    bitmap_min(bitmap);
                }
                break;
            }

            case OP_MAX: {
                if (!bitmap_is_empty(bitmap)) {
                    bitmap_max(bitmap);
                }
                break;
            }

            case OP_OPTIMIZE: {
                bool shrink = fuzz_consume_bool(&input);
                bitmap_optimize(bitmap, shrink);
                break;
            }

            case OP_STATISTICS: {
                int format = fuzz_consume_u8(&input) % 2;
                int size_out;
                char* stats = bitmap_statistics_str(bitmap, format, &size_out);
                safe_free(stats);
                break;
            }

            case OP_CARDINALITY: {
                bitmap_get_cardinality(bitmap);
                break;
            }

            case OP_ISEMPTY: {
                bitmap_is_empty(bitmap);
                break;
            }

            case OP_GETBITS: {
                size_t n_offsets = fuzz_consume_size_in_range(&input, 1, 100);
                uint32_t* offsets = (uint32_t*)malloc(n_offsets * sizeof(uint32_t));
                if (offsets) {
                    for (size_t i = 0; i < n_offsets && fuzz_input_remaining(&input) > sizeof(uint32_t); i++) {
                        offsets[i] = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                    }
                    bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
                    safe_free(results);
                    safe_free(offsets);
                }
                break;
            }

            case OP_CLEARBITS: {
                size_t n_offsets = fuzz_consume_size_in_range(&input, 1, 100);
                uint32_t* offsets = (uint32_t*)malloc(n_offsets * sizeof(uint32_t));
                if (offsets) {
                    for (size_t i = 0; i < n_offsets && fuzz_input_remaining(&input) > sizeof(uint32_t); i++) {
                        offsets[i] = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                    }
                    bitmap_clearbits(bitmap, n_offsets, offsets);
                    safe_free(offsets);
                }
                break;
            }

            case OP_CLEARBITS_COUNT: {
                size_t n_offsets = fuzz_consume_size_in_range(&input, 1, 100);
                uint32_t* offsets = (uint32_t*)malloc(n_offsets * sizeof(uint32_t));
                if (offsets) {
                    for (size_t i = 0; i < n_offsets && fuzz_input_remaining(&input) > sizeof(uint32_t); i++) {
                        offsets[i] = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                    }
                    bitmap_clearbits_count(bitmap, n_offsets, offsets);
                    safe_free(offsets);
                }
                break;
            }

            case OP_INTERSECT: {
                /* Create temporary second bitmap */
                Bitmap* bitmap2 = bitmap_alloc();
                if (bitmap2) {
                    for (int i = 0; i < 10 && fuzz_input_remaining(&input) > sizeof(uint32_t); i++) {
                        uint32_t offset = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                        bitmap_setbit(bitmap2, offset, true);
                    }
                    uint32_t mode = fuzz_consume_u8(&input) % 4;
                    bitmap_intersect(bitmap, bitmap2, mode);
                    bitmap_free(bitmap2);
                }
                break;
            }

            case OP_JACCARD: {
                /* Create temporary second bitmap */
                Bitmap* bitmap2 = bitmap_alloc();
                if (bitmap2) {
                    for (int i = 0; i < 10 && fuzz_input_remaining(&input) > sizeof(uint32_t); i++) {
                        uint32_t offset = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                        bitmap_setbit(bitmap2, offset, true);
                    }
                    bitmap_jaccard(bitmap, bitmap2);
                    bitmap_free(bitmap2);
                }
                break;
            }

            case OP_GET_NTH_ELEMENT: {
                uint64_t n = fuzz_consume_u64(&input);
                if (n < 1000000) {
                    bitmap_get_nth_element_present(bitmap, n);
                    bitmap_get_nth_element_not_present(bitmap, n);
                }
                break;
            }

            case OP_FREE_AND_ALLOC: {
                /* Stress test allocation/deallocation */
                Bitmap* temp = bitmap_alloc();
                if (temp) {
                    bitmap_free(temp);
                }
                break;
            }

            default:
                break;
        }
    }

    bitmap_free(bitmap);
    return 0;
}
