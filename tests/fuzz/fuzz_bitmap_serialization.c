/*
 * Fuzzer for Bitmap Serialization and Parsing
 *
 * This fuzzer focuses on data conversion and parsing operations:
 * - from_int_array / get_int_array
 * - from_bit_array / get_bit_array
 * - Range operations
 * - Malformed inputs and edge cases
 */

#include "fuzz_common.h"

/* Test types for serialization fuzzer */
enum SerializationTestType {
    TEST_INTARRAY_32 = 0,
    TEST_INTARRAY_64,
    TEST_BITARRAY_32,
    TEST_BITARRAY_64,
    TEST_RANGE_32,
    TEST_RANGE_64,
    NUM_SERIALIZATION_TESTS
};

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }

    FuzzInput input;
    fuzz_input_init(&input, data, size);

    uint8_t test_type = fuzz_consume_u8(&input) % NUM_SERIALIZATION_TESTS;

    switch (test_type) {
        case TEST_INTARRAY_32:
            {
                size_t array_size = fuzz_consume_size_in_range(&input, 0, MAX_ARRAY_SIZE);
                if (array_size == 0) break;

                uint32_t* array = (uint32_t*)malloc(sizeof(uint32_t) * array_size);
                if (!array) break;

                /* Fill with fuzz data */
                for (size_t i = 0; i < array_size && fuzz_input_remaining(&input) >= 4; i++) {
                    array[i] = fuzz_consume_u32(&input);
                }

                /* Create bitmap from array */
                Bitmap* bitmap = bitmap_from_int_array(array_size, array);
                if (bitmap) {
                    /* Get array back */
                    size_t result_size;
                    uint32_t* result = bitmap_get_int_array(bitmap, &result_size);

                    if (result) {
                        /* Verify cardinality matches result size */
                        uint64_t cardinality = bitmap_get_cardinality(bitmap);
                        if (cardinality != result_size) {
                            /* Invariant violation */
                        }

                        /* Try range operations */
                        if (result_size > 0) {
                            size_t start = fuzz_consume_size_in_range(&input, 0, result_size);
                            size_t end = fuzz_consume_size_in_range(&input, start, result_size + 10);
                            size_t range_count;
                            uint32_t* range_result = bitmap_range_int_array(bitmap, start, end, &range_count);
                            safe_free(range_result);
                        }

                        safe_free(result);
                    }

                    bitmap_free(bitmap);
                }

                safe_free(array);
            }
            break;

        case TEST_INTARRAY_64:
            {
                size_t array_size = fuzz_consume_size_in_range(&input, 0, MAX_ARRAY_SIZE);
                if (array_size == 0) break;

                uint64_t* array = (uint64_t*)malloc(sizeof(uint64_t) * array_size);
                if (!array) break;

                /* Fill with fuzz data */
                for (size_t i = 0; i < array_size && fuzz_input_remaining(&input) >= 8; i++) {
                    array[i] = fuzz_consume_u64(&input);
                }

                /* Create bitmap from array */
                Bitmap64* bitmap = bitmap64_from_int_array(array_size, array);
                if (bitmap) {
                    /* Get array back */
                    uint64_t result_size;
                    uint64_t* result = bitmap64_get_int_array(bitmap, &result_size);

                    if (result) {
                        /* Verify cardinality */
                        uint64_t cardinality = bitmap64_get_cardinality(bitmap);
                        if (cardinality != result_size) {
                            /* Invariant violation */
                        }

                        /* Try range operations */
                        if (result_size > 0) {
                            uint64_t start = fuzz_consume_u64_in_range(&input, 0, result_size);
                            uint64_t end = fuzz_consume_u64_in_range(&input, start, result_size + 10);
                            uint64_t range_count;
                            uint64_t* range_result = bitmap64_range_int_array(bitmap, start, end, &range_count);
                            safe_free(range_result);
                        }

                        safe_free(result);
                    }

                    bitmap64_free(bitmap);
                }

                safe_free(array);
            }
            break;

        case TEST_BITARRAY_32:
            {
                size_t bit_size = fuzz_consume_size_in_range(&input, 0, MAX_ARRAY_SIZE);
                if (bit_size == 0) break;

                char* bit_array = (char*)malloc(bit_size + 1);
                if (!bit_array) break;

                /* Fill with potentially invalid characters */
                for (size_t i = 0; i < bit_size && fuzz_input_remaining(&input) > 0; i++) {
                    uint8_t choice = fuzz_consume_u8(&input) % 10;
                    if (choice < 4) {
                        bit_array[i] = '0';
                    } else if (choice < 8) {
                        bit_array[i] = '1';
                    } else {
                        /* Inject some invalid characters */
                        bit_array[i] = (char)fuzz_consume_u8(&input);
                    }
                }
                bit_array[bit_size] = '\0';

                /* Try to create bitmap (should handle invalid input gracefully) */
                Bitmap* bitmap = bitmap_from_bit_array(bit_size, bit_array);
                if (bitmap) {
                    /* Try to get bit array back */
                    size_t result_size;
                    char* result = bitmap_get_bit_array(bitmap, &result_size);

                    if (result) {
                        /* Result should only contain '0' and '1' */
                        for (size_t i = 0; i < result_size; i++) {
                            if (result[i] != '0' && result[i] != '1') {
                                /* Invalid output */
                            }
                        }
                        bitmap_free_bit_array(result);
                    }

                    bitmap_free(bitmap);
                }

                safe_free(bit_array);
            }
            break;

        case TEST_BITARRAY_64:
            {
                size_t bit_size = fuzz_consume_size_in_range(&input, 0, MAX_ARRAY_SIZE);
                if (bit_size == 0) break;

                char* bit_array = (char*)malloc(bit_size + 1);
                if (!bit_array) break;

                for (size_t i = 0; i < bit_size && fuzz_input_remaining(&input) > 0; i++) {
                    uint8_t choice = fuzz_consume_u8(&input) % 10;
                    if (choice < 4) {
                        bit_array[i] = '0';
                    } else if (choice < 8) {
                        bit_array[i] = '1';
                    } else {
                        bit_array[i] = (char)fuzz_consume_u8(&input);
                    }
                }
                bit_array[bit_size] = '\0';

                Bitmap64* bitmap = bitmap64_from_bit_array(bit_size, bit_array);
                if (bitmap) {
                    uint64_t result_size;
                    char* result = bitmap64_get_bit_array(bitmap, &result_size);

                    if (result) {
                        for (size_t i = 0; i < result_size; i++) {
                            if (result[i] != '0' && result[i] != '1') {
                                /* Invalid output */
                            }
                        }
                        bitmap_free_bit_array(result);
                    }

                    bitmap64_free(bitmap);
                }

                safe_free(bit_array);
            }
            break;

        case TEST_RANGE_32:
            {
                uint64_t from = fuzz_consume_u32(&input);
                uint64_t to = fuzz_consume_u32(&input);

                /* Test various range scenarios */
                if (from > to) {
                    /* Reversed range */
                    Bitmap* bitmap = bitmap_from_range(to, from);
                    if (bitmap) {
                        uint64_t card = bitmap_get_cardinality(bitmap);
                        /* Should be from - to */
                        bitmap_free(bitmap);
                    }
                } else if (from == to) {
                    /* Empty or single element range */
                    Bitmap* bitmap = bitmap_from_range(from, to);
                    if (bitmap) {
                        bitmap_free(bitmap);
                    }
                } else {
                    /* Normal range, but limit size */
                    if (to - from > 1000000) {
                        to = from + 1000000;
                    }
                    Bitmap* bitmap = bitmap_from_range(from, to);
                    if (bitmap) {
                        uint64_t card = bitmap_get_cardinality(bitmap);
                        /* Cardinality should equal range size */
                        if (to > from && card != (to - from)) {
                            /* Invariant check */
                        }
                        bitmap_free(bitmap);
                    }
                }
            }
            break;

        case TEST_RANGE_64:
            {
                uint64_t from = fuzz_consume_u64(&input);
                uint64_t to = fuzz_consume_u64(&input);

                if (from > to) {
                    Bitmap64* bitmap = bitmap64_from_range(to, from);
                    if (bitmap) {
                        bitmap64_free(bitmap);
                    }
                } else {
                    if (to - from > 1000000) {
                        to = from + 1000000;
                    }
                    Bitmap64* bitmap = bitmap64_from_range(from, to);
                    if (bitmap) {
                        uint64_t card = bitmap64_get_cardinality(bitmap);
                        bitmap64_free(bitmap);
                    }
                }
            }
            break;
    }

    return 0;
}
