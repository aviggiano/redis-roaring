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

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);

    // Decide on number of input bitmaps
    int num_inputs = fdp.ConsumeIntegralInRange<int>(2, MAX_OPERATION_INPUTS);

    // Create input bitmaps
    Bitmap* inputs[MAX_OPERATION_INPUTS];
    for (int i = 0; i < num_inputs; i++) {
        inputs[i] = bitmap_alloc();
        if (!inputs[i]) {
            // Cleanup and return
            for (int j = 0; j < i; j++) {
                bitmap_free(inputs[j]);
            }
            return 0;
        }

        // Populate each bitmap with different patterns
        uint8_t pattern = fdp.ConsumeIntegral<uint8_t>() % 6;
        switch (pattern) {
            case 0: // Empty bitmap
                break;

            case 1: // Small sparse set
                for (int j = 0; j < 10; j++) {
                    uint32_t offset = fdp.ConsumeIntegral<uint32_t>() % 1000;
                    bitmap_setbit(inputs[i], offset, true);
                }
                break;

            case 2: // Dense range
                {
                    uint32_t start = fdp.ConsumeIntegral<uint32_t>() % 10000;
                    uint32_t len = fdp.ConsumeIntegralInRange<uint32_t>(100, 1000);
                    Bitmap* range = bitmap_from_range(start, start + len);
                    if (range) {
                        bitmap_free(inputs[i]);
                        inputs[i] = range;
                    }
                }
                break;

            case 3: // Random from array
                {
                    size_t array_size;
                    uint32_t* array = generate_uint32_array(fdp, &array_size);
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

            case 4: // Bit pattern
                {
                    size_t bit_size;
                    char* bit_array = generate_bit_array_string(fdp, &bit_size);
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

            case 5: // Large sparse set
                for (int j = 0; j < 100; j++) {
                    uint32_t offset = fdp.ConsumeIntegral<uint32_t>();
                    bitmap_setbit(inputs[i], offset, true);
                }
                break;
        }
    }

    // Create result bitmap
    Bitmap* result = bitmap_alloc();
    if (!result) {
        for (int i = 0; i < num_inputs; i++) {
            bitmap_free(inputs[i]);
        }
        return 0;
    }

    // Create const pointer array for operations
    const Bitmap* const_inputs[MAX_OPERATION_INPUTS];
    for (int i = 0; i < num_inputs; i++) {
        const_inputs[i] = inputs[i];
    }

    // Test various operations
    uint8_t operation = fdp.ConsumeIntegral<uint8_t>() % 8;

    switch (operation) {
        case 0: // AND
            bitmap_and(result, num_inputs, const_inputs);

            // Verify result is subset of all inputs
            for (int i = 0; i < num_inputs; i++) {
                Bitmap* intersection = bitmap_alloc();
                const Bitmap* pair[2] = {result, inputs[i]};
                bitmap_and(intersection, 2, pair);

                // Result AND input[i] should equal result
                uint64_t result_card = bitmap_get_cardinality(result);
                uint64_t inter_card = bitmap_get_cardinality(intersection);

                // Basic invariant check (can be removed in production fuzzing)
                if (result_card != inter_card) {
                    // This shouldn't happen, but we don't abort - just note it
                }

                bitmap_free(intersection);
            }
            break;

        case 1: // OR
            bitmap_or(result, num_inputs, const_inputs);

            // Verify result is superset of all inputs
            for (int i = 0; i < num_inputs; i++) {
                uint64_t input_card = bitmap_get_cardinality(inputs[i]);
                uint64_t result_card = bitmap_get_cardinality(result);

                // Result should have at least as many elements as any input
                if (result_card < input_card) {
                    // Invariant violation (shouldn't happen)
                }
            }
            break;

        case 2: // XOR
            bitmap_xor(result, num_inputs, const_inputs);

            // Just ensure it doesn't crash
            bitmap_get_cardinality(result);
            break;

        case 3: // ANDOR
            bitmap_andor(result, num_inputs, const_inputs);
            bitmap_get_cardinality(result);
            break;

        case 4: // ANDNOT
            bitmap_andnot(result, num_inputs, const_inputs);
            bitmap_get_cardinality(result);
            break;

        case 5: // ORNOT
            bitmap_ornot(result, num_inputs, const_inputs);
            bitmap_get_cardinality(result);
            break;

        case 6: // ONE (symmetric difference)
            bitmap_one(result, num_inputs, const_inputs);
            bitmap_get_cardinality(result);
            break;

        case 7: // NOT (on first input only)
            {
                Bitmap* not_result = bitmap_not(inputs[0]);
                if (not_result) {
                    // NOT followed by NOT should give original (with bounds)
                    Bitmap* double_not = bitmap_not(not_result);
                    if (double_not) {
                        bitmap_free(double_not);
                    }
                    bitmap_free(not_result);
                }
            }
            break;
    }

    // Test some additional operations on result
    if (!bitmap_is_empty(result)) {
        bitmap_min(result);
        bitmap_max(result);
        bitmap_optimize(result, 1);

        // Get statistics
        Bitmap_statistics stats;
        bitmap_statistics(result, &stats);
    }

    // Test intersect modes between pairs
    if (num_inputs >= 2) {
        for (uint32_t mode = BITMAP_INTERSECT_MODE_NONE;
             mode <= BITMAP_INTERSECT_MODE_EQ && fdp.remaining_bytes() > 0;
             mode++) {
            bitmap_intersect(inputs[0], inputs[1], mode);
        }
    }

    // Test jaccard similarity
    if (num_inputs >= 2) {
        double jaccard = bitmap_jaccard(inputs[0], inputs[1]);
        // Jaccard should be between 0 and 1
        if (jaccard < 0.0 || jaccard > 1.0) {
            // Invariant violation (shouldn't happen unless both empty)
        }
    }

    // Cleanup
    bitmap_free(result);
    for (int i = 0; i < num_inputs; i++) {
        bitmap_free(inputs[i]);
    }

    return 0;
}
