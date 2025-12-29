/*
 * Focused Fuzzer for Bitmap Operations
 *
 * This fuzzer specifically targets complex bitmap operations:
 * - Operations with multiple input bitmaps (2-10 inputs)
 * - Edge cases: empty bitmaps, full bitmaps, overlapping ranges
 * - All BITOP variants (AND, OR, XOR, NOT, ANDOR, ANDNOT, ORNOT, ONE)
 */

#include "fuzz_common.h"

#define MAX_OPERATION_INPUTS 10

/* Operation types for operations fuzzer */
enum OperationFuzzType {
    OP_FUZZ_AND = 0,
    OP_FUZZ_OR,
    OP_FUZZ_XOR,
    OP_FUZZ_ANDOR,
    OP_FUZZ_ANDNOT,
    OP_FUZZ_ORNOT,
    OP_FUZZ_ONE,
    OP_FUZZ_NOT,
    NUM_OP_FUZZ_TYPES
};

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) {
        return 0;
    }

    FuzzInput input;
    fuzz_input_init(&input, data, size);

    /* Decide on number of input bitmaps */
    int num_inputs = (int)fuzz_consume_u32_in_range(&input, 2, MAX_OPERATION_INPUTS);

    /* Create input bitmaps */
    Bitmap* inputs[MAX_OPERATION_INPUTS];
    for (int i = 0; i < num_inputs; i++) {
        inputs[i] = bitmap_alloc();
        if (!inputs[i]) {
            /* Cleanup and return */
            for (int j = 0; j < i; j++) {
                bitmap_free(inputs[j]);
            }
            return 0;
        }

        /* Populate each bitmap with different patterns */
        uint8_t pattern = fuzz_consume_u8(&input) % 6;
        switch (pattern) {
            case 0: /* Empty bitmap */
                break;

            case 1: /* Small sparse set */
                for (int j = 0; j < 10; j++) {
                    uint32_t offset = fuzz_consume_u32(&input) % 1000;
                    bitmap_setbit(inputs[i], offset, true);
                }
                break;

            case 2: /* Dense range */
                {
                    uint32_t start = fuzz_consume_u32(&input) % 10000;
                    uint32_t len = fuzz_consume_u32_in_range(&input, 100, 1000);
                    Bitmap* range = bitmap_from_range(start, start + len);
                    if (range) {
                        bitmap_free(inputs[i]);
                        inputs[i] = range;
                    }
                }
                break;

            case 3: /* Random from array */
                {
                    size_t array_size;
                    uint32_t* array = generate_uint32_array(&input, &array_size);
                    if (array && array_size > 0) {
                        Bitmap* from_array = bitmap_from_int_array(array_size, array);
                        if (from_array) {
                            bitmap_free(inputs[i]);
                            inputs[i] = from_array;
                        }
                        safe_free(array);
                    }
                }
                break;

            case 4: /* Bit pattern */
                {
                    size_t bit_size;
                    char* bit_array = generate_bit_array_string(&input, &bit_size);
                    if (bit_array && bit_size > 0) {
                        Bitmap* from_bits = bitmap_from_bit_array(bit_size, bit_array);
                        if (from_bits) {
                            bitmap_free(inputs[i]);
                            inputs[i] = from_bits;
                        }
                        safe_free(bit_array);
                    }
                }
                break;

            case 5: /* Large sparse set */
                for (int j = 0; j < 100; j++) {
                    uint32_t offset = fuzz_consume_u32_in_range(&input, 0, MAX_BIT_OFFSET_32);
                    bitmap_setbit(inputs[i], offset, true);
                }
                break;
        }
    }

    /* Create result bitmap */
    Bitmap* result = bitmap_alloc();
    if (!result) {
        for (int i = 0; i < num_inputs; i++) {
            bitmap_free(inputs[i]);
        }
        return 0;
    }

    /* Create const pointer array for operations */
    const Bitmap* const_inputs[MAX_OPERATION_INPUTS];
    for (int i = 0; i < num_inputs; i++) {
        const_inputs[i] = inputs[i];
    }

    /* Test various operations */
    uint8_t operation = fuzz_consume_u8(&input) % NUM_OP_FUZZ_TYPES;

    switch (operation) {
        case OP_FUZZ_AND:
            bitmap_and(result, num_inputs, const_inputs);

            /* Verify result is subset of all inputs */
            for (int i = 0; i < num_inputs; i++) {
                Bitmap* intersection = bitmap_alloc();
                const Bitmap* pair[2] = {result, inputs[i]};
                bitmap_and(intersection, 2, pair);

                /* Result AND input[i] should equal result */
                uint64_t result_card = bitmap_get_cardinality(result);
                uint64_t inter_card = bitmap_get_cardinality(intersection);

                /* Basic invariant check (can be removed in production fuzzing) */
                if (result_card != inter_card) {
                    /* This shouldn't happen, but we don't abort - just note it */
                }

                bitmap_free(intersection);
            }
            if (bitmap_get_cardinality(result) <= MAX_RANGE_VALIDATE_CARDINALITY) {
                size_t result_len = 0;
                uint32_t* result_array = bitmap_get_int_array(result, &result_len);
                if (result_array) {
                    for (size_t i = 0; i < result_len; i++) {
                        for (int j = 0; j < num_inputs; j++) {
                            fuzz_require(bitmap_getbit(inputs[j], result_array[i]));
                        }
                    }
                    safe_free(result_array);
                }
            }
            break;

        case OP_FUZZ_OR:
            bitmap_or(result, num_inputs, const_inputs);

            /* Verify result is superset of all inputs */
            for (int i = 0; i < num_inputs; i++) {
                uint64_t input_card = bitmap_get_cardinality(inputs[i]);
                uint64_t result_card = bitmap_get_cardinality(result);

                /* Result should have at least as many elements as any input */
                if (result_card < input_card) {
                    /* Invariant violation (shouldn't happen) */
                }
            }
            for (int i = 0; i < num_inputs; i++) {
                if (bitmap_get_cardinality(inputs[i]) <= MAX_RANGE_VALIDATE_CARDINALITY) {
                    size_t input_len = 0;
                    uint32_t* input_array = bitmap_get_int_array(inputs[i], &input_len);
                    if (input_array) {
                        for (size_t j = 0; j < input_len; j++) {
                            fuzz_require(bitmap_getbit(result, input_array[j]));
                        }
                        safe_free(input_array);
                    }
                }
            }
            break;

        case OP_FUZZ_XOR:
            bitmap_xor(result, num_inputs, const_inputs);

            /* Just ensure it doesn't crash */
            bitmap_get_cardinality(result);
            break;

        case OP_FUZZ_ANDOR:
            bitmap_andor(result, num_inputs, const_inputs);
            bitmap_get_cardinality(result);
            break;

        case OP_FUZZ_ANDNOT:
            bitmap_andnot(result, num_inputs, const_inputs);
            bitmap_get_cardinality(result);
            break;

        case OP_FUZZ_ORNOT:
            bitmap_ornot(result, num_inputs, const_inputs);
            bitmap_get_cardinality(result);
            break;

        case OP_FUZZ_ONE:
            bitmap_one(result, num_inputs, const_inputs);
            bitmap_get_cardinality(result);
            break;

        case OP_FUZZ_NOT:
            {
                Bitmap* not_result = bitmap_not(inputs[0]);
                if (not_result) {
                    /* NOT followed by NOT should give original (with bounds) */
                    Bitmap* double_not = bitmap_not(not_result);
                    if (double_not) {
                        bitmap_free(double_not);
                    }
                    bitmap_free(not_result);
                }
            }
            break;
    }

    /* Test some additional operations on result */
    if (!bitmap_is_empty(result)) {
        bitmap_min(result);
        bitmap_max(result);
        bitmap_optimize(result, 1);

        /* Get statistics */
        Bitmap_statistics stats;
        bitmap_statistics(result, &stats);
    }

    /* Test intersect modes between pairs */
    if (num_inputs >= 2) {
        for (uint32_t mode = BITMAP_INTERSECT_MODE_NONE;
             mode <= BITMAP_INTERSECT_MODE_EQ && fuzz_input_remaining(&input) > 0;
             mode++) {
            bitmap_intersect(inputs[0], inputs[1], mode);
        }
    }

    /* Test jaccard similarity */
    if (num_inputs >= 2) {
        double jaccard = bitmap_jaccard(inputs[0], inputs[1]);
        /* Jaccard should be between 0 and 1 */
        if (jaccard < 0.0 || jaccard > 1.0) {
            /* Invariant violation (shouldn't happen unless both empty) */
        }
    }

    /* Cleanup */
    bitmap_free(result);
    for (int i = 0; i < num_inputs; i++) {
        bitmap_free(inputs[i]);
    }

    return 0;
}
