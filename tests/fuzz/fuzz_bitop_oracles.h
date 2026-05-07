#ifndef REDIS_ROARING_FUZZ_BITOP_ORACLES_H
#define REDIS_ROARING_FUZZ_BITOP_ORACLES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuzz_common.h"
#include "fuzz_redis_harness.h"

typedef enum {
  FUZZ_BITOP_AND = 0,
  FUZZ_BITOP_OR,
  FUZZ_BITOP_XOR,
  FUZZ_BITOP_NOT,
  FUZZ_BITOP_ANDOR,
  FUZZ_BITOP_ONE,
  FUZZ_BITOP_DIFF,
  FUZZ_BITOP_DIFF1,
  FUZZ_BITOP_INVALID,
} FuzzBitOpKind;

static inline const char* fuzz_bitop_kind_name(FuzzBitOpKind kind) {
  switch (kind) {
    case FUZZ_BITOP_AND: return "AND";
    case FUZZ_BITOP_OR: return "OR";
    case FUZZ_BITOP_XOR: return "XOR";
    case FUZZ_BITOP_NOT: return "NOT";
    case FUZZ_BITOP_ANDOR: return "ANDOR";
    case FUZZ_BITOP_ONE: return "ONE";
    case FUZZ_BITOP_DIFF: return "DIFF";
    case FUZZ_BITOP_DIFF1: return "DIFF1";
    case FUZZ_BITOP_INVALID: return "NOOP";
  }

  return "NOOP";
}

static inline FuzzBitOpKind fuzz_consume_bitop_kind(FuzzInput* input) {
  return (FuzzBitOpKind)fuzz_consume_u32_in_range(input, 0, FUZZ_BITOP_INVALID);
}

static inline bool fuzz_bitop_kind_is_variadic(FuzzBitOpKind kind) {
  return kind == FUZZ_BITOP_AND
      || kind == FUZZ_BITOP_OR
      || kind == FUZZ_BITOP_XOR
      || kind == FUZZ_BITOP_ANDOR
      || kind == FUZZ_BITOP_ONE
      || kind == FUZZ_BITOP_DIFF
      || kind == FUZZ_BITOP_DIFF1;
}

static inline size_t fuzz_generate_values32(FuzzInput* input, uint32_t* values, size_t max_count, uint32_t max_value) {
  size_t count = fuzz_consume_size_in_range(input, 0, max_count);
  for (size_t i = 0; i < count; i++) {
    values[i] = fuzz_consume_u32_in_range(input, 0, max_value);
  }
  return count;
}

static inline size_t fuzz_generate_values64(FuzzInput* input, uint64_t* values, size_t max_count, uint64_t max_value) {
  size_t count = fuzz_consume_size_in_range(input, 0, max_count);
  for (size_t i = 0; i < count; i++) {
    values[i] = fuzz_consume_u64_in_range(input, 0, max_value);
  }
  return count;
}

static inline Bitmap* fuzz_bitmap32_from_values(size_t count, const uint32_t* values) {
  if (count == 0) {
    return bitmap_alloc();
  }
  return bitmap_from_int_array(count, values);
}

static inline Bitmap64* fuzz_bitmap64_from_values(size_t count, const uint64_t* values) {
  if (count == 0) {
    return bitmap64_alloc();
  }
  return bitmap64_from_int_array(count, values);
}

static inline void fuzz_seed_key32(FuzzRedisServer* server, const char* key, size_t count, const uint32_t* values) {
  if (count > 0) {
    fuzz_redis_set_int_array32(server, "R.SETINTARRAY", key, count, values);
  }
}

static inline void fuzz_seed_key64(FuzzRedisServer* server, const char* key, size_t count, const uint64_t* values) {
  if (count > 0) {
    fuzz_redis_set_int_array64(server, "R64.SETINTARRAY", key, count, values);
  }
}

static inline Bitmap* fuzz_bitop_expected32(FuzzBitOpKind kind, size_t source_count, const Bitmap** sources,
                                            bool has_last, uint32_t last_arg) {
  if (kind == FUZZ_BITOP_INVALID) {
    return NULL;
  }

  if (kind == FUZZ_BITOP_NOT) {
    fuzz_require(source_count == 1);
    const Bitmap* source = sources[0];
    long long last = -1;
    if (has_last) {
      last = (long long)last_arg;
    }
    if (!bitmap_is_empty(source)) {
      long long max = (long long)bitmap_max(source);
      if (last < max) {
        last = max;
      }
    }
    return bitmap_flip(source, (uint32_t)(last + 1));
  }

  Bitmap* result = bitmap_alloc();
  switch (kind) {
    case FUZZ_BITOP_AND:
      bitmap_and(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_OR:
      bitmap_or(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_XOR:
      bitmap_xor(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_ANDOR:
      bitmap_andor(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_ONE:
      bitmap_one(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_DIFF:
      bitmap_andnot(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_DIFF1:
      bitmap_ornot(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_NOT:
    case FUZZ_BITOP_INVALID:
      break;
  }

  return result;
}

static inline Bitmap64* fuzz_bitop_expected64(FuzzBitOpKind kind, size_t source_count, const Bitmap64** sources,
                                              bool has_last, uint64_t last_arg) {
  if (kind == FUZZ_BITOP_INVALID) {
    return NULL;
  }

  if (kind == FUZZ_BITOP_NOT) {
    fuzz_require(source_count == 1);
    const Bitmap64* source = sources[0];
    bool has_max = !bitmap64_is_empty(source);
    if (!has_last && !has_max) {
      return bitmap64_alloc();
    }

    uint64_t last = has_last ? last_arg : 0;
    if (has_max) {
      uint64_t max = bitmap64_max(source);
      if (!has_last || last < max) {
        last = max;
      }
    }
    return bitmap64_flip(source, last + 1);
  }

  Bitmap64* result = bitmap64_alloc();
  switch (kind) {
    case FUZZ_BITOP_AND:
      bitmap64_and(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_OR:
      bitmap64_or(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_XOR:
      bitmap64_xor(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_ANDOR:
      bitmap64_andor(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_ONE:
      bitmap64_one(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_DIFF:
      bitmap64_andnot(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_DIFF1:
      bitmap64_ornot(result, (uint32_t)source_count, sources);
      break;
    case FUZZ_BITOP_NOT:
    case FUZZ_BITOP_INVALID:
      break;
  }

  return result;
}

static inline void fuzz_require_bitmap32_matches_key(FuzzRedisServer* server, const char* key, const Bitmap* expected) {
  size_t expected_count = 0;
  size_t actual_count = 0;
  uint32_t* expected_values = bitmap_get_int_array(expected, &expected_count);
  uint32_t* actual_values = fuzz_redis_get_int_array32(server, key, &actual_count);

  fuzz_require_uint32_arrays_equal(expected_values, expected_count, actual_values, actual_count);

  safe_free(expected_values);
  safe_free(actual_values);
}

static inline void fuzz_require_bitmap64_matches_key(FuzzRedisServer* server, const char* key, const Bitmap64* expected) {
  uint64_t expected_count64 = 0;
  size_t actual_count = 0;
  uint64_t* expected_values = bitmap64_get_int_array(expected, &expected_count64);
  uint64_t* actual_values = fuzz_redis_get_int_array64(server, key, &actual_count);

  fuzz_require_uint64_arrays_equal(expected_values, (size_t)expected_count64, actual_values, actual_count);

  safe_free(expected_values);
  safe_free(actual_values);
}

#endif
