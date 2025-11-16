/*
 * Fuzzer for 64-bit Bitmap API
 *
 * This fuzzer tests the 64-bit redis-roaring bitmap operations by:
 * - Maintaining multiple 64-bit bitmap instances
 * - Performing random sequences of operations
 * - Testing edge cases with large uint64_t values
 */

#include "fuzz_common.h"

// Fuzzer state for 64-bit bitmaps
struct Fuzzer64State {
    Bitmap64* bitmaps[MAX_FUZZ_BITMAPS];
    int num_bitmaps;

    Fuzzer64State() : num_bitmaps(0) {
        for (int i = 0; i < MAX_FUZZ_BITMAPS; i++) {
            bitmaps[i] = nullptr;
        }
    }

    ~Fuzzer64State() {
        cleanup();
    }

    void cleanup() {
        for (int i = 0; i < num_bitmaps; i++) {
            if (bitmaps[i]) {
                bitmap64_free(bitmaps[i]);
                bitmaps[i] = nullptr;
            }
        }
        num_bitmaps = 0;
    }

    int add_bitmap() {
        if (num_bitmaps >= MAX_FUZZ_BITMAPS) {
            return -1;
        }
        bitmaps[num_bitmaps] = bitmap64_alloc();
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
    if (size < 4) {
        return 0;
    }

    FuzzedDataProvider fdp(data, size);
    Fuzzer64State state;

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

        uint8_t op = fdp.ConsumeIntegral<uint8_t>() % NUM_BITMAP64_OPERATIONS;
        int idx = select_bitmap_index(fdp, state.num_bitmaps);
        Bitmap64* bitmap = state.bitmaps[idx];

        if (!bitmap) continue;

        switch (op) {
            case OP64_SETBIT: {
                uint64_t offset = fdp.ConsumeIntegral<uint64_t>();
                bool value = fdp.ConsumeBool();
                bitmap64_setbit(bitmap, offset, value);
                break;
            }

            case OP64_GETBIT: {
                uint64_t offset = fdp.ConsumeIntegral<uint64_t>();
                bitmap64_getbit(bitmap, offset);
                break;
            }

            case OP64_SETINTARRAY: {
                size_t array_size;
                uint64_t* array = generate_uint64_array(fdp, &array_size);
                if (array && array_size > 0) {
                    Bitmap64* new_bitmap = bitmap64_from_int_array(array_size, array);
                    if (new_bitmap) {
                        bitmap64_free(bitmap);
                        state.bitmaps[idx] = new_bitmap;
                    }
                    safe_free(array);
                }
                break;
            }

            case OP64_GETINTARRAY: {
                uint64_t result_size;
                uint64_t* result = bitmap64_get_int_array(bitmap, &result_size);
                safe_free(result);
                break;
            }

            case OP64_APPENDINTARRAY: {
                size_t array_size;
                uint64_t* array = generate_uint64_array(fdp, &array_size);
                if (array && array_size > 0) {
                    Bitmap64* new_values = bitmap64_from_int_array(array_size, array);
                    if (new_values) {
                        const Bitmap64* inputs[2] = {bitmap, new_values};
                        bitmap64_or(bitmap, 2, inputs);
                        bitmap64_free(new_values);
                    }
                    safe_free(array);
                }
                break;
            }

            case OP64_RANGEINTARRAY: {
                uint64_t card = bitmap64_get_cardinality(bitmap);
                if (card > 0) {
                    uint64_t start = fdp.ConsumeIntegralInRange<uint64_t>(0, card);
                    uint64_t end = fdp.ConsumeIntegralInRange<uint64_t>(start, card + 100);
                    uint64_t result_count;
                    uint64_t* result = bitmap64_range_int_array(bitmap, start, end, &result_count);
                    safe_free(result);
                }
                break;
            }

            case OP64_SETBITARRAY: {
                size_t bit_array_size;
                char* bit_array = generate_bit_array_string(fdp, &bit_array_size);
                if (bit_array && bit_array_size > 0) {
                    Bitmap64* new_bitmap = bitmap64_from_bit_array(bit_array_size, bit_array);
                    if (new_bitmap) {
                        bitmap64_free(bitmap);
                        state.bitmaps[idx] = new_bitmap;
                    }
                    safe_free(bit_array);
                }
                break;
            }

            case OP64_GETBITARRAY: {
                uint64_t size;
                char* bit_array = bitmap64_get_bit_array(bitmap, &size);
                bitmap_free_bit_array(bit_array);
                break;
            }

            case OP64_SETRANGE: {
                uint64_t from = fdp.ConsumeIntegral<uint64_t>();
                uint64_t to = fdp.ConsumeIntegral<uint64_t>();
                if (from > to) {
                    uint64_t temp = from;
                    from = to;
                    to = temp;
                }
                // Limit range size to avoid excessive memory
                if (to - from > 1000000) {
                    to = from + 1000000;
                }
                Bitmap64* range_bitmap = bitmap64_from_range(from, to);
                if (range_bitmap) {
                    bitmap64_free(bitmap);
                    state.bitmaps[idx] = range_bitmap;
                }
                break;
            }

            case OP64_BITOP_AND: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap64* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap64* result = bitmap64_alloc();
                    bitmap64_and(result, 2, inputs);
                    bitmap64_free(result);
                }
                break;
            }

            case OP64_BITOP_OR: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap64* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap64* result = bitmap64_alloc();
                    bitmap64_or(result, 2, inputs);
                    bitmap64_free(result);
                }
                break;
            }

            case OP64_BITOP_XOR: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap64* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap64* result = bitmap64_alloc();
                    bitmap64_xor(result, 2, inputs);
                    bitmap64_free(result);
                }
                break;
            }

            case OP64_BITOP_NOT: {
                Bitmap64* result = bitmap64_not(bitmap);
                if (result) {
                    bitmap64_free(result);
                }
                break;
            }

            case OP64_BITOP_ANDOR: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap64* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap64* result = bitmap64_alloc();
                    bitmap64_andor(result, 2, inputs);
                    bitmap64_free(result);
                }
                break;
            }

            case OP64_BITOP_ANDNOT: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap64* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap64* result = bitmap64_alloc();
                    bitmap64_andnot(result, 2, inputs);
                    bitmap64_free(result);
                }
                break;
            }

            case OP64_BITOP_ORNOT: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap64* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap64* result = bitmap64_alloc();
                    bitmap64_ornot(result, 2, inputs);
                    bitmap64_free(result);
                }
                break;
            }

            case OP64_BITOP_ONE: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    const Bitmap64* inputs[2] = {bitmap, state.bitmaps[idx2]};
                    Bitmap64* result = bitmap64_alloc();
                    bitmap64_one(result, 2, inputs);
                    bitmap64_free(result);
                }
                break;
            }

            case OP64_MIN: {
                if (!bitmap64_is_empty(bitmap)) {
                    bitmap64_min(bitmap);
                }
                break;
            }

            case OP64_MAX: {
                if (!bitmap64_is_empty(bitmap)) {
                    bitmap64_max(bitmap);
                }
                break;
            }

            case OP64_OPTIMIZE: {
                int shrink = fdp.ConsumeBool() ? 1 : 0;
                bitmap64_optimize(bitmap, shrink);
                break;
            }

            case OP64_STATISTICS: {
                Bitmap64_statistics stats;
                bitmap64_statistics(bitmap, &stats);

                int format = fdp.ConsumeBool() ? BITMAP_STATISTICS_FORMAT_JSON : BITMAP_STATISTICS_FORMAT_PLAIN_TEXT;
                int size_out;
                char* stats_str = bitmap64_statistics_str(bitmap, format, &size_out);
                safe_free(stats_str);
                break;
            }

            case OP64_CARDINALITY: {
                bitmap64_get_cardinality(bitmap);
                break;
            }

            case OP64_ISEMPTY: {
                bitmap64_is_empty(bitmap);
                break;
            }

            case OP64_GETBITS: {
                size_t n_offsets = fdp.ConsumeIntegralInRange<size_t>(1, 100);
                uint64_t* offsets = (uint64_t*)malloc(sizeof(uint64_t) * n_offsets);
                if (offsets) {
                    for (size_t i = 0; i < n_offsets; i++) {
                        offsets[i] = fdp.ConsumeIntegral<uint64_t>();
                    }
                    bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
                    safe_free(results);
                    safe_free(offsets);
                }
                break;
            }

            case OP64_CLEARBITS: {
                size_t n_offsets = fdp.ConsumeIntegralInRange<size_t>(1, 100);
                uint64_t* offsets = (uint64_t*)malloc(sizeof(uint64_t) * n_offsets);
                if (offsets) {
                    for (size_t i = 0; i < n_offsets; i++) {
                        offsets[i] = fdp.ConsumeIntegral<uint64_t>();
                    }
                    bitmap64_clearbits(bitmap, n_offsets, offsets);
                    safe_free(offsets);
                }
                break;
            }

            case OP64_CLEARBITS_COUNT: {
                size_t n_offsets = fdp.ConsumeIntegralInRange<size_t>(1, 100);
                uint64_t* offsets = (uint64_t*)malloc(sizeof(uint64_t) * n_offsets);
                if (offsets) {
                    for (size_t i = 0; i < n_offsets; i++) {
                        offsets[i] = fdp.ConsumeIntegral<uint64_t>();
                    }
                    bitmap64_clearbits_count(bitmap, n_offsets, offsets);
                    safe_free(offsets);
                }
                break;
            }

            case OP64_INTERSECT: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    uint32_t mode = fdp.ConsumeIntegralInRange<uint32_t>(
                        BITMAP_INTERSECT_MODE_NONE,
                        BITMAP_INTERSECT_MODE_EQ
                    );
                    bitmap64_intersect(bitmap, state.bitmaps[idx2], mode);
                }
                break;
            }

            case OP64_JACCARD: {
                if (state.num_bitmaps >= 2) {
                    int idx2 = select_bitmap_index(fdp, state.num_bitmaps);
                    bitmap64_jaccard(bitmap, state.bitmaps[idx2]);
                }
                break;
            }

            case OP64_GET_NTH_ELEMENT: {
                uint64_t card = bitmap64_get_cardinality(bitmap);
                if (card > 0) {
                    uint64_t n = fdp.ConsumeIntegralInRange<uint64_t>(0, card + 10);
                    bool found;
                    bitmap64_get_nth_element_present(bitmap, n, &found);
                    bitmap64_get_nth_element_not_present(bitmap, n, &found);
                }
                break;
            }

            case OP64_FREE_AND_ALLOC: {
                bitmap64_free(bitmap);
                state.bitmaps[idx] = bitmap64_alloc();
                break;
            }

            default:
                break;
        }
    }

    return 0;
}
