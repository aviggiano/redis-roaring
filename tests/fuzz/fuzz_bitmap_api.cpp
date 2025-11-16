/*
 * Fuzzer for 32-bit Bitmap API
 *
 * This fuzzer tests the main redis-roaring bitmap operations by:
 * - Maintaining multiple bitmap instances
 * - Performing random sequences of operations
 * - Testing edge cases and error handling
 */

#include "fuzz_common.h"

// Fuzzer state
struct FuzzerState {
    Bitmap* bitmaps[MAX_FUZZ_BITMAPS];
    int num_bitmaps;

    FuzzerState() : num_bitmaps(0) {
        for (int i = 0; i < MAX_FUZZ_BITMAPS; i++) {
            bitmaps[i] = nullptr;
        }
    }

    ~FuzzerState() {
        cleanup();
    }

    void cleanup() {
        for (int i = 0; i < num_bitmaps; i++) {
            if (bitmaps[i]) {
                bitmap_free(bitmaps[i]);
                bitmaps[i] = nullptr;
            }
        }
        num_bitmaps = 0;
    }

    int add_bitmap() {
        if (num_bitmaps >= MAX_FUZZ_BITMAPS) {
            return -1;
        }
        bitmaps[num_bitmaps] = bitmap_alloc();
        if (!bitmaps[num_bitmaps]) {
            return -1;
        }
        return num_bitmaps++;
    }

    bool has_bitmaps() const {
        return num_bitmaps > 0;
    }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Need at least a few bytes to do anything interesting
    if (size < 4) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);
    FuzzerState state;

    // Initialize with 1-5 bitmaps
    int initial_bitmaps = fdp.ConsumeIntegralInRange<int>(1, MAX_FUZZ_BITMAPS);
    for (int i = 0; i < initial_bitmaps; i++) {
        state.add_bitmap();
    }

    // Perform random operations
    int operations = 0;
    while (fdp.remaining_bytes() > 0 && operations < MAX_FUZZ_OPERATIONS) {
        operations++;

        if (!state.has_bitmaps()) {
            break;
        }

        uint8_t op = fdp.ConsumeIntegral<uint8_t>() % NUM_BITMAP_OPERATIONS;
        int idx = select_bitmap_index(fdp, state.num_bitmaps);
        Bitmap* bitmap = state.bitmaps[idx];

        if (!bitmap) continue;

        switch (op) {
            case OP_SETBIT: {
                uint32_t offset = fdp.ConsumeIntegral<uint32_t>();
                bool value = fdp.ConsumeBool();
                bitmap_setbit(bitmap, offset, value);
                break;
            }

            case OP_GETBIT: {
                uint32_t offset = fdp.ConsumeIntegral<uint32_t>();
                bitmap_getbit(bitmap, offset);
                break;
            }

            case OP_SETINTARRAY: {
                size_t array_size;
                uint32_t* array = generate_uint32_array(fdp, &array_size);
                if (array && array_size > 0) {
                    Bitmap* new_bitmap = bitmap_from_int_array(array_size, array);
                    if (new_bitmap) {
                        bitmap_free(bitmap);
                        state.bitmaps[idx] = new_bitmap;
                    }
                    safe_free(array);
                }
                break;
            }

            case OP_GETINTARRAY: {
                size_t result_size;
                uint32_t* result = bitmap_get_int_array(bitmap, &result_size);
                safe_free(result);
                break;
            }

            case OP_APPENDINTARRAY: {
                size_t array_size;
                uint32_t* array = generate_uint32_array(fdp, &array_size);
                if (array && array_size > 0) {
                    // Append by creating new bitmap and OR-ing
                    Bitmap* new_values = bitmap_from_int_array(array_size, array);
                    if (new_values) {
                        const Bitmap* inputs[2] = {bitmap, new_values};
                        bitmap_or(bitmap, 2, inputs);
                        bitmap_free(new_values);
                    }
                    safe_free(array);
                }
                break;
            }

            case OP_RANGEINTARRAY: {
                uint64_t card = bitmap_get_cardinality(bitmap);
                if (card > 0) {
                    size_t start = fdp.ConsumeIntegralInRange<size_t>(0, card);
                    size_t end = fdp.ConsumeIntegralInRange<size_t>(start, card + 100);
                    size_t result_count;
                    uint32_t* result = bitmap_range_int_array(bitmap, start, end, &result_count);
                    safe_free(result);
                }
                break;
            }

            case OP_SETBITARRAY: {
                size_t bit_array_size;
                char* bit_array = generate_bit_array_string(fdp, &bit_array_size);
                if (bit_array && bit_array_size > 0) {
                    Bitmap* new_bitmap = bitmap_from_bit_array(bit_array_size, bit_array);
                    if (new_bitmap) {
                        bitmap_free(bitmap);
                        state.bitmaps[idx] = new_bitmap;
                    }
                    safe_free(bit_array);
                }
                break;
            }

            case OP_GETBITARRAY: {
                size_t size;
                char* bit_array = bitmap_get_bit_array(bitmap, &size);
                bitmap_free_bit_array(bit_array);
                break;
            }

            case OP_SETRANGE: {
                uint64_t from = fdp.ConsumeIntegral<uint32_t>();
                uint64_t to = fdp.ConsumeIntegral<uint32_t>();
                if (from > to) {
                    uint64_t temp = from;
                    from = to;
                    to = temp;
                }
                // Limit range size to avoid excessive memory
                if (to - from > 1000000) {
                    to = from + 1000000;
                }
                Bitmap* range_bitmap = bitmap_from_range(from, to);
                if (range_bitmap) {
                    bitmap_free(bitmap);
                    state.bitmaps[idx] = range_bitmap;
                }
                break;
            }

            case OP_BITOP_AND: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap* result = bitmap_alloc();
                    bitmap_and(result, 2, inputs);
                    bitmap_free(result);
                }
                break;
            }

            case OP_BITOP_OR: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap* result = bitmap_alloc();
                    bitmap_or(result, 2, inputs);
                    bitmap_free(result);
                }
                break;
            }

            case OP_BITOP_XOR: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap* result = bitmap_alloc();
                    bitmap_xor(result, 2, inputs);
                    bitmap_free(result);
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

            case OP_BITOP_ANDOR: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap* result = bitmap_alloc();
                    bitmap_andor(result, 2, inputs);
                    bitmap_free(result);
                }
                break;
            }

            case OP_BITOP_ANDNOT: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap* result = bitmap_alloc();
                    bitmap_andnot(result, 2, inputs);
                    bitmap_free(result);
                }
                break;
            }

            case OP_BITOP_ORNOT: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap* result = bitmap_alloc();
                    bitmap_ornot(result, 2, inputs);
                    bitmap_free(result);
                }
                break;
            }

            case OP_BITOP_ONE: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap* result = bitmap_alloc();
                    bitmap_one(result, 2, inputs);
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
                int shrink = fdp.ConsumeBool() ? 1 : 0;
                bitmap_optimize(bitmap, shrink);
                break;
            }

            case OP_STATISTICS: {
                Bitmap_statistics stats;
                bitmap_statistics(bitmap, &stats);

                int format = fdp.ConsumeBool() ? BITMAP_STATISTICS_FORMAT_JSON : BITMAP_STATISTICS_FORMAT_PLAIN_TEXT;
                int size_out;
                char* stats_str = bitmap_statistics_str(bitmap, format, &size_out);
                safe_free(stats_str);
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
                size_t n_offsets = fdp.ConsumeIntegralInRange<size_t>(1, 100);
                uint32_t* offsets = (uint32_t*)malloc(sizeof(uint32_t) * n_offsets);
                if (offsets) {
                    for (size_t i = 0; i < n_offsets; i++) {
                        offsets[i] = fdp.ConsumeIntegral<uint32_t>();
                    }
                    bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
                    safe_free(results);
                    safe_free(offsets);
                }
                break;
            }

            case OP_CLEARBITS: {
                size_t n_offsets = fdp.ConsumeIntegralInRange<size_t>(1, 100);
                uint32_t* offsets = (uint32_t*)malloc(sizeof(uint32_t) * n_offsets);
                if (offsets) {
                    for (size_t i = 0; i < n_offsets; i++) {
                        offsets[i] = fdp.ConsumeIntegral<uint32_t>();
                    }
                    bitmap_clearbits(bitmap, n_offsets, offsets);
                    safe_free(offsets);
                }
                break;
            }

            case OP_CLEARBITS_COUNT: {
                size_t n_offsets = fdp.ConsumeIntegralInRange<size_t>(1, 100);
                uint32_t* offsets = (uint32_t*)malloc(sizeof(uint32_t) * n_offsets);
                if (offsets) {
                    for (size_t i = 0; i < n_offsets; i++) {
                        offsets[i] = fdp.ConsumeIntegral<uint32_t>();
                    }
                    bitmap_clearbits_count(bitmap, n_offsets, offsets);
                    safe_free(offsets);
                }
                break;
            }

            case OP_INTERSECT: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    uint32_t mode = fdp.ConsumeIntegralInRange<uint32_t>(
                        BITMAP_INTERSECT_MODE_NONE,
                        BITMAP_INTERSECT_MODE_EQ
                    );
                    bitmap_intersect(bitmap, state.bitmaps[idx2], mode);
                }
                break;
            }

            case OP_JACCARD: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    bitmap_jaccard(bitmap, state.bitmaps[idx2]);
                }
                break;
            }

            case OP_GET_NTH_ELEMENT: {
                uint64_t card = bitmap_get_cardinality(bitmap);
                if (card > 0) {
                    uint64_t n = fdp.ConsumeIntegralInRange<uint64_t>(0, card + 10);
                    bitmap_get_nth_element_present(bitmap, n);
                    bitmap_get_nth_element_not_present(bitmap, n);
                }
                break;
            }

            case OP_FREE_AND_ALLOC: {
                bitmap_free(bitmap);
                state.bitmaps[idx] = bitmap_alloc();
                break;
            }

            default:
                break;
        }
    }

    return 0;
}
