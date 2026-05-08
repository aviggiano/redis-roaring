#define _GNU_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_bitop_oracles.h"
#include "fuzz_common.h"

#define FUZZ_PARITY_MAX_KEYS 4
#define FUZZ_PARITY_MAX_ARGS 16
#define FUZZ_PARITY_MAX_VALUES 16
#define FUZZ_PARITY_MAX_ARG_BUFFERS 32
#define FUZZ_PARITY_SAFE_VALUE_MAX 255
#define FUZZ_PARITY_SHARED_RANGE_END 31

typedef enum {
  FUZZ_PARITY_SETBIT = 0,
  FUZZ_PARITY_GETBIT,
  FUZZ_PARITY_GETBITS,
  FUZZ_PARITY_CLEARBITS,
  FUZZ_PARITY_SETINTARRAY,
  FUZZ_PARITY_GETINTARRAY,
  FUZZ_PARITY_RANGEINTARRAY,
  FUZZ_PARITY_APPENDINTARRAY,
  FUZZ_PARITY_DELETEINTARRAY,
  FUZZ_PARITY_DIFF,
  FUZZ_PARITY_SETFULL,
  FUZZ_PARITY_SETRANGE,
  FUZZ_PARITY_OPTIMIZE,
  FUZZ_PARITY_SETBITARRAY,
  FUZZ_PARITY_GETBITARRAY,
  FUZZ_PARITY_BITOP,
  FUZZ_PARITY_BITCOUNT,
  FUZZ_PARITY_BITPOS,
  FUZZ_PARITY_MIN,
  FUZZ_PARITY_MAX,
  FUZZ_PARITY_CLEAR,
  FUZZ_PARITY_CONTAINS,
  FUZZ_PARITY_JACCARD,
} FuzzParityCommandKind;

typedef enum {
  FUZZ_PARITY_STATE_FULL = 0,
  FUZZ_PARITY_STATE_SHARED_RANGE,
  FUZZ_PARITY_STATE_SHARED_BITS,
} FuzzParityStateMode;

typedef struct {
  const char* command32;
  const char* command64;
  FuzzParityCommandKind kind;
  bool readonly;
} FuzzParityCommandSpec;

typedef struct {
  const FuzzParityCommandSpec* spec;
  const char* argv32[FUZZ_PARITY_MAX_ARGS];
  const char* argv64[FUZZ_PARITY_MAX_ARGS];
  int argc32;
  int argc64;
  bool seed_keys[FUZZ_PARITY_MAX_KEYS];
  FuzzParityStateMode key_modes[FUZZ_PARITY_MAX_KEYS];
  size_t seed_counts[FUZZ_PARITY_MAX_KEYS];
  uint32_t seed_values[FUZZ_PARITY_MAX_KEYS][FUZZ_PARITY_MAX_VALUES];
  char arg_buffers[FUZZ_PARITY_MAX_ARG_BUFFERS][32];
  size_t arg_buffer_count;
  char bit_array[65];
} FuzzParityCase;

typedef enum {
  FUZZ_PARITY_ALIAS_DISTINCT = 0,
  FUZZ_PARITY_ALIAS_DEST_EQ_SRC0,
  FUZZ_PARITY_ALIAS_REPEAT_SOURCE,
} FuzzParityBitOpAliasMode;

typedef enum {
  FUZZ_PARITY_DIFF_DISTINCT = 0,
  FUZZ_PARITY_DIFF_DEST_EQ_SRC0,
  FUZZ_PARITY_DIFF_DEST_EQ_SRC1,
} FuzzParityDiffAliasMode;

static const FuzzParityCommandSpec FUZZ_PARITY_COMMANDS[] = {
    {"R.SETBIT", "R64.SETBIT", FUZZ_PARITY_SETBIT, false},
    {"R.GETBIT", "R64.GETBIT", FUZZ_PARITY_GETBIT, true},
    {"R.GETBITS", "R64.GETBITS", FUZZ_PARITY_GETBITS, true},
    {"R.CLEARBITS", "R64.CLEARBITS", FUZZ_PARITY_CLEARBITS, false},
    {"R.SETINTARRAY", "R64.SETINTARRAY", FUZZ_PARITY_SETINTARRAY, false},
    {"R.GETINTARRAY", "R64.GETINTARRAY", FUZZ_PARITY_GETINTARRAY, true},
    {"R.RANGEINTARRAY", "R64.RANGEINTARRAY", FUZZ_PARITY_RANGEINTARRAY, true},
    {"R.APPENDINTARRAY", "R64.APPENDINTARRAY", FUZZ_PARITY_APPENDINTARRAY, false},
    {"R.DELETEINTARRAY", "R64.DELETEINTARRAY", FUZZ_PARITY_DELETEINTARRAY, false},
    {"R.DIFF", "R64.DIFF", FUZZ_PARITY_DIFF, false},
    {"R.SETFULL", "R64.SETFULL", FUZZ_PARITY_SETFULL, false},
    {"R.SETRANGE", "R64.SETRANGE", FUZZ_PARITY_SETRANGE, false},
    {"R.OPTIMIZE", "R64.OPTIMIZE", FUZZ_PARITY_OPTIMIZE, true},
    {"R.SETBITARRAY", "R64.SETBITARRAY", FUZZ_PARITY_SETBITARRAY, false},
    {"R.GETBITARRAY", "R64.GETBITARRAY", FUZZ_PARITY_GETBITARRAY, true},
    {"R.BITOP", "R64.BITOP", FUZZ_PARITY_BITOP, false},
    {"R.BITCOUNT", "R64.BITCOUNT", FUZZ_PARITY_BITCOUNT, true},
    {"R.BITPOS", "R64.BITPOS", FUZZ_PARITY_BITPOS, true},
    {"R.MIN", "R64.MIN", FUZZ_PARITY_MIN, true},
    {"R.MAX", "R64.MAX", FUZZ_PARITY_MAX, true},
    {"R.CLEAR", "R64.CLEAR", FUZZ_PARITY_CLEAR, false},
    {"R.CONTAINS", "R64.CONTAINS", FUZZ_PARITY_CONTAINS, true},
    {"R.JACCARD", "R64.JACCARD", FUZZ_PARITY_JACCARD, true},
};

static const char* FUZZ_PARITY_KEYS32[FUZZ_PARITY_MAX_KEYS] = {"r:key0", "r:key1", "r:key2", "r:key3"};
static const char* FUZZ_PARITY_KEYS64[FUZZ_PARITY_MAX_KEYS] = {"r64:key0", "r64:key1", "r64:key2", "r64:key3"};
static const FuzzParityCommandKind FUZZ_PARITY_SHARED_RANGE_ALLOWLIST[] = {FUZZ_PARITY_SETFULL};

static FuzzRedisServer FUZZ_PARITY_SERVER;
static bool FUZZ_PARITY_SERVER_READY = false;

static void fuzz_parity_cleanup(void) {
  if (FUZZ_PARITY_SERVER_READY) {
    fuzz_redis_shutdown(&FUZZ_PARITY_SERVER, false);
    FUZZ_PARITY_SERVER_READY = false;
  }
}

static FuzzRedisServer* fuzz_parity_server(void) {
  if (!FUZZ_PARITY_SERVER_READY) {
    fuzz_require(fuzz_redis_start(&FUZZ_PARITY_SERVER, false, false, false));
    atexit(fuzz_parity_cleanup);
    FUZZ_PARITY_SERVER_READY = true;
  }

  return &FUZZ_PARITY_SERVER;
}

static void fuzz_parity_case_init(FuzzParityCase* parity_case, const FuzzParityCommandSpec* spec) {
  memset(parity_case, 0, sizeof(*parity_case));
  parity_case->spec = spec;
  for (size_t i = 0; i < FUZZ_PARITY_MAX_KEYS; i++) {
    parity_case->key_modes[i] = FUZZ_PARITY_STATE_FULL;
  }
}

static bool fuzz_parity_command_is_shared_range_allowlisted(FuzzParityCommandKind kind) {
  for (size_t i = 0; i < sizeof(FUZZ_PARITY_SHARED_RANGE_ALLOWLIST) / sizeof(FUZZ_PARITY_SHARED_RANGE_ALLOWLIST[0]);
       i++) {
    if (FUZZ_PARITY_SHARED_RANGE_ALLOWLIST[i] == kind) {
      return true;
    }
  }

  return false;
}

static char* fuzz_parity_next_buffer(FuzzParityCase* parity_case) {
  fuzz_require(parity_case->arg_buffer_count < FUZZ_PARITY_MAX_ARG_BUFFERS);
  return parity_case->arg_buffers[parity_case->arg_buffer_count++];
}

static const char* fuzz_parity_push_u32(FuzzParityCase* parity_case, uint32_t value) {
  char* buffer = fuzz_parity_next_buffer(parity_case);
  snprintf(buffer, 32, "%u", value);
  return buffer;
}

static const char* fuzz_parity_push_u64(FuzzParityCase* parity_case, uint64_t value) {
  char* buffer = fuzz_parity_next_buffer(parity_case);
  snprintf(buffer, 32, "%llu", (unsigned long long)value);
  return buffer;
}

static void fuzz_parity_fill_seed_values(FuzzInput* input, FuzzParityCase* parity_case, size_t key_index,
                                         bool allow_empty) {
  size_t count = allow_empty ? fuzz_consume_size_in_range(input, 0, FUZZ_PARITY_MAX_VALUES)
                             : 1 + fuzz_consume_size_in_range(input, 0, FUZZ_PARITY_MAX_VALUES - 1);

  parity_case->seed_keys[key_index] = true;
  parity_case->seed_counts[key_index] = count;
  for (size_t i = 0; i < count; i++) {
    parity_case->seed_values[key_index][i] = fuzz_consume_u32_in_range(input, 0, FUZZ_PARITY_SAFE_VALUE_MAX);
  }
}

static void fuzz_parity_copy_seed_values(FuzzParityCase* parity_case, size_t dst_index, size_t src_index) {
  parity_case->seed_keys[dst_index] = parity_case->seed_keys[src_index];
  parity_case->seed_counts[dst_index] = parity_case->seed_counts[src_index];
  memcpy(parity_case->seed_values[dst_index],
         parity_case->seed_values[src_index],
         parity_case->seed_counts[src_index] * sizeof(parity_case->seed_values[dst_index][0]));
}

static void fuzz_parity_seed_key_pair(FuzzRedisServer* server, size_t count, const uint32_t* values32,
                                      const char* key32, const char* key64) {
  if (count == 0) {
    return;
  }

  uint64_t values64[FUZZ_PARITY_MAX_VALUES];
  for (size_t i = 0; i < count; i++) {
    values64[i] = values32[i];
  }

  fuzz_redis_set_int_array32(server, "R.SETINTARRAY", key32, count, values32);
  fuzz_redis_set_int_array64(server, "R64.SETINTARRAY", key64, count, values64);
}

static void fuzz_parity_seed_case(FuzzRedisServer* server, const FuzzParityCase* parity_case) {
  for (size_t i = 0; i < FUZZ_PARITY_MAX_KEYS; i++) {
    if (parity_case->seed_keys[i]) {
      fuzz_parity_seed_key_pair(
          server,
          parity_case->seed_counts[i],
          parity_case->seed_values[i],
          FUZZ_PARITY_KEYS32[i],
          FUZZ_PARITY_KEYS64[i]);
    }
  }
}

static void fuzz_parity_compare_reply(redisReply* lhs, redisReply* rhs) {
  fuzz_reply_require(lhs);
  fuzz_reply_require(rhs);
  fuzz_require(lhs->type == rhs->type);

  switch (lhs->type) {
    case REDIS_REPLY_INTEGER:
    case REDIS_REPLY_BOOL:
      fuzz_require(lhs->integer == rhs->integer);
      return;
    case REDIS_REPLY_DOUBLE:
      fuzz_require(lhs->dval == rhs->dval);
      fuzz_require(lhs->str != NULL);
      fuzz_require(rhs->str != NULL);
      fuzz_require(strcmp(lhs->str, rhs->str) == 0);
      return;
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_BIGNUM:
      fuzz_require(lhs->str != NULL || rhs->str == NULL);
      fuzz_require(rhs->str != NULL || lhs->str == NULL);
      if (lhs->str == NULL) {
        return;
      }
      fuzz_require(strcmp(lhs->str, rhs->str) == 0);
      return;
    case REDIS_REPLY_VERB:
      fuzz_require(strcmp(lhs->vtype, rhs->vtype) == 0);
      fuzz_require(lhs->str != NULL || rhs->str == NULL);
      fuzz_require(rhs->str != NULL || lhs->str == NULL);
      if (lhs->str == NULL) {
        return;
      }
      fuzz_require(strcmp(lhs->str, rhs->str) == 0);
      return;
    case REDIS_REPLY_ARRAY:
    case REDIS_REPLY_SET:
    case REDIS_REPLY_PUSH:
    case REDIS_REPLY_MAP:
    case REDIS_REPLY_ATTR:
      fuzz_require(lhs->elements == rhs->elements);
      for (size_t i = 0; i < lhs->elements; i++) {
        fuzz_parity_compare_reply(lhs->element[i], rhs->element[i]);
      }
      return;
    case REDIS_REPLY_NIL:
      return;
  }

  fuzz_require(false);
}

static void fuzz_parity_compare_key_pair_full(FuzzRedisServer* server, const char* key32, const char* key64) {
  size_t count32 = 0;
  size_t count64 = 0;
  uint32_t* values32 = fuzz_redis_get_int_array32(server, key32, &count32);
  uint64_t* values64 = fuzz_redis_get_int_array64(server, key64, &count64);

  fuzz_require(count32 == count64);
  for (size_t i = 0; i < count32; i++) {
    fuzz_require(values32[i] == values64[i]);
  }

  safe_free(values32);
  safe_free(values64);
}

static uint32_t* fuzz_parity_get_range32(FuzzRedisServer* server, const char* key, size_t start, size_t end,
                                         size_t* count) {
  redisReply* reply = fuzz_redis_command(server, "R.RANGEINTARRAY %s %zu %zu", key, start, end);
  fuzz_reply_require(reply);
  fuzz_require(reply->type == REDIS_REPLY_ARRAY);

  uint32_t* values = malloc(reply->elements * sizeof(*values));
  fuzz_require(values != NULL || reply->elements == 0);
  for (size_t i = 0; i < reply->elements; i++) {
    fuzz_require(reply->element[i]->type == REDIS_REPLY_INTEGER);
    fuzz_require(reply->element[i]->integer >= 0);
    values[i] = (uint32_t)reply->element[i]->integer;
  }

  *count = reply->elements;
  fuzz_redis_free_reply(reply);
  return values;
}

static uint64_t* fuzz_parity_get_range64(FuzzRedisServer* server, const char* key, uint64_t start, uint64_t end,
                                         size_t* count) {
  redisReply* reply = fuzz_redis_command(
      server, "R64.RANGEINTARRAY %s %llu %llu", key, (unsigned long long)start, (unsigned long long)end);
  fuzz_reply_require(reply);
  fuzz_require(reply->type == REDIS_REPLY_ARRAY);

  uint64_t* values = malloc(reply->elements * sizeof(*values));
  fuzz_require(values != NULL || reply->elements == 0);
  for (size_t i = 0; i < reply->elements; i++) {
    fuzz_require(reply->element[i]->type == REDIS_REPLY_INTEGER);
    fuzz_require(reply->element[i]->integer >= 0);
    values[i] = (uint64_t)reply->element[i]->integer;
  }

  *count = reply->elements;
  fuzz_redis_free_reply(reply);
  return values;
}

static void fuzz_parity_compare_shared_bits(FuzzRedisServer* server, const char* key32, const char* key64) {
  const uint32_t sample_offsets[] = {0, 1, 2, 7, 15, 31, 63, 127, 255};
  for (size_t i = 0; i < sizeof(sample_offsets) / sizeof(sample_offsets[0]); i++) {
    redisReply* reply32 = fuzz_redis_command(server, "R.GETBIT %s %u", key32, sample_offsets[i]);
    redisReply* reply64 = fuzz_redis_command(server, "R64.GETBIT %s %u", key64, sample_offsets[i]);
    fuzz_parity_compare_reply(reply32, reply64);
    fuzz_redis_free_reply(reply32);
    fuzz_redis_free_reply(reply64);
  }
}

static void fuzz_parity_compare_key_pair_shared_range(FuzzRedisServer* server, const char* key32, const char* key64) {
  size_t count32 = 0;
  size_t count64 = 0;
  uint32_t* values32 = fuzz_parity_get_range32(server, key32, 0, FUZZ_PARITY_SHARED_RANGE_END, &count32);
  uint64_t* values64 = fuzz_parity_get_range64(server, key64, 0, FUZZ_PARITY_SHARED_RANGE_END, &count64);

  fuzz_require(count32 == count64);
  for (size_t i = 0; i < count32; i++) {
    fuzz_require(values32[i] == values64[i]);
  }

  safe_free(values32);
  safe_free(values64);

  fuzz_parity_compare_shared_bits(server, key32, key64);
}

static void fuzz_parity_compare_case_state(FuzzRedisServer* server, const FuzzParityCase* parity_case) {
  for (size_t i = 0; i < FUZZ_PARITY_MAX_KEYS; i++) {
    if (parity_case->key_modes[i] == FUZZ_PARITY_STATE_SHARED_RANGE) {
      fuzz_parity_compare_key_pair_shared_range(server, FUZZ_PARITY_KEYS32[i], FUZZ_PARITY_KEYS64[i]);
    } else if (parity_case->key_modes[i] == FUZZ_PARITY_STATE_SHARED_BITS) {
      fuzz_parity_compare_shared_bits(server, FUZZ_PARITY_KEYS32[i], FUZZ_PARITY_KEYS64[i]);
    } else {
      fuzz_parity_compare_key_pair_full(server, FUZZ_PARITY_KEYS32[i], FUZZ_PARITY_KEYS64[i]);
    }
  }
}

static void fuzz_parity_build_bit_array(FuzzInput* input, FuzzParityCase* parity_case) {
  size_t len = 1 + fuzz_consume_size_in_range(input, 0, sizeof(parity_case->bit_array) - 2);
  for (size_t i = 0; i < len; i++) {
    parity_case->bit_array[i] = fuzz_consume_bool(input) ? '1' : '0';
  }
  parity_case->bit_array[len] = '\0';
}

static void fuzz_parity_append_key_arg(FuzzParityCase* parity_case, size_t key_index) {
  parity_case->argv32[parity_case->argc32++] = FUZZ_PARITY_KEYS32[key_index];
  parity_case->argv64[parity_case->argc64++] = FUZZ_PARITY_KEYS64[key_index];
}

static void fuzz_parity_build_case(FuzzInput* input, FuzzParityCase* parity_case) {
  const FuzzParityCommandSpec* spec = parity_case->spec;

  parity_case->argv32[parity_case->argc32++] = spec->command32;
  parity_case->argv64[parity_case->argc64++] = spec->command64;

  switch (spec->kind) {
    case FUZZ_PARITY_SETBIT:
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_append_key_arg(parity_case, 0);
      parity_case->argv32[parity_case->argc32++] =
          fuzz_parity_push_u32(parity_case, fuzz_consume_u32_in_range(input, 0, FUZZ_PARITY_SAFE_VALUE_MAX));
      parity_case->argv64[parity_case->argc64++] = parity_case->argv32[parity_case->argc32 - 1];
      parity_case->argv32[parity_case->argc32++] = fuzz_consume_bool(input) ? "1" : "0";
      parity_case->argv64[parity_case->argc64++] = parity_case->argv32[parity_case->argc32 - 1];
      return;
    case FUZZ_PARITY_GETBIT:
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_append_key_arg(parity_case, 0);
      parity_case->argv32[parity_case->argc32++] =
          fuzz_parity_push_u32(parity_case, fuzz_consume_u32_in_range(input, 0, FUZZ_PARITY_SAFE_VALUE_MAX));
      parity_case->argv64[parity_case->argc64++] = parity_case->argv32[parity_case->argc32 - 1];
      return;
    case FUZZ_PARITY_GETBITS:
    case FUZZ_PARITY_CLEARBITS: {
      size_t offset_count = 1 + fuzz_consume_size_in_range(input, 0, 3);
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_append_key_arg(parity_case, 0);
      for (size_t i = 0; i < offset_count; i++) {
        const char* arg =
            fuzz_parity_push_u32(parity_case, fuzz_consume_u32_in_range(input, 0, FUZZ_PARITY_SAFE_VALUE_MAX));
        parity_case->argv32[parity_case->argc32++] = arg;
        parity_case->argv64[parity_case->argc64++] = arg;
      }
      if (spec->kind == FUZZ_PARITY_CLEARBITS && fuzz_consume_bool(input)) {
        parity_case->argv32[parity_case->argc32++] = "COUNT";
        parity_case->argv64[parity_case->argc64++] = "COUNT";
      }
      return;
    }
    case FUZZ_PARITY_SETINTARRAY:
    case FUZZ_PARITY_APPENDINTARRAY:
    case FUZZ_PARITY_DELETEINTARRAY: {
      size_t value_count = 1 + fuzz_consume_size_in_range(input, 0, 3);
      fuzz_parity_fill_seed_values(input, parity_case, 0, spec->kind == FUZZ_PARITY_SETINTARRAY);
      fuzz_parity_append_key_arg(parity_case, 0);
      for (size_t i = 0; i < value_count; i++) {
        const char* arg =
            fuzz_parity_push_u32(parity_case, fuzz_consume_u32_in_range(input, 0, FUZZ_PARITY_SAFE_VALUE_MAX));
        parity_case->argv32[parity_case->argc32++] = arg;
        parity_case->argv64[parity_case->argc64++] = arg;
      }
      return;
    }
    case FUZZ_PARITY_GETINTARRAY:
    case FUZZ_PARITY_BITCOUNT:
    case FUZZ_PARITY_MIN:
    case FUZZ_PARITY_MAX:
    case FUZZ_PARITY_CLEAR:
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_append_key_arg(parity_case, 0);
      return;
    case FUZZ_PARITY_RANGEINTARRAY: {
      uint32_t start;
      uint32_t end;
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_append_key_arg(parity_case, 0);
      start = fuzz_consume_u32_in_range(input, 0, 8);
      end = start + fuzz_consume_u32_in_range(input, 0, 8);
      parity_case->argv32[parity_case->argc32++] = fuzz_parity_push_u32(parity_case, start);
      parity_case->argv64[parity_case->argc64++] = parity_case->argv32[parity_case->argc32 - 1];
      parity_case->argv32[parity_case->argc32++] = fuzz_parity_push_u32(parity_case, end);
      parity_case->argv64[parity_case->argc64++] = parity_case->argv32[parity_case->argc32 - 1];
      return;
    }
    case FUZZ_PARITY_DIFF: {
      FuzzParityDiffAliasMode alias_mode =
          (FuzzParityDiffAliasMode)fuzz_consume_u32_in_range(input, 0, FUZZ_PARITY_DIFF_DEST_EQ_SRC1);
      size_t source0 = 1;
      size_t source1 = 2;

      if (alias_mode == FUZZ_PARITY_DIFF_DEST_EQ_SRC0) {
        source0 = 0;
      } else if (alias_mode == FUZZ_PARITY_DIFF_DEST_EQ_SRC1) {
        source1 = 0;
      }

      if (fuzz_consume_bool(input)) {
        fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      }
      if (source0 == 0) {
        if (!parity_case->seed_keys[0]) {
          fuzz_parity_fill_seed_values(input, parity_case, 0, false);
        }
      } else {
        fuzz_parity_fill_seed_values(input, parity_case, source0, false);
      }
      if (source1 == 0) {
        if (!parity_case->seed_keys[0]) {
          fuzz_parity_fill_seed_values(input, parity_case, 0, false);
        }
      } else if (source1 == source0) {
        fuzz_parity_copy_seed_values(parity_case, source1, source0);
      } else {
        fuzz_parity_fill_seed_values(input, parity_case, source1, false);
      }

      fuzz_parity_append_key_arg(parity_case, 0);
      fuzz_parity_append_key_arg(parity_case, source0);
      fuzz_parity_append_key_arg(parity_case, source1);
      return;
    }
    case FUZZ_PARITY_SETFULL:
      fuzz_require(fuzz_parity_command_is_shared_range_allowlisted(spec->kind));
      parity_case->key_modes[0] = FUZZ_PARITY_STATE_SHARED_BITS;
      fuzz_parity_append_key_arg(parity_case, 0);
      return;
    case FUZZ_PARITY_SETRANGE: {
      uint32_t start = fuzz_consume_u32_in_range(input, 0, 64);
      uint32_t end = start + fuzz_consume_u32_in_range(input, 0, 64);
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_append_key_arg(parity_case, 0);
      parity_case->argv32[parity_case->argc32++] = fuzz_parity_push_u32(parity_case, start);
      parity_case->argv64[parity_case->argc64++] = parity_case->argv32[parity_case->argc32 - 1];
      parity_case->argv32[parity_case->argc32++] = fuzz_parity_push_u32(parity_case, end);
      parity_case->argv64[parity_case->argc64++] = parity_case->argv32[parity_case->argc32 - 1];
      return;
    }
    case FUZZ_PARITY_OPTIMIZE:
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_append_key_arg(parity_case, 0);
      if (fuzz_consume_bool(input)) {
        parity_case->argv32[parity_case->argc32++] = "MEM";
        parity_case->argv64[parity_case->argc64++] = "MEM";
      }
      return;
    case FUZZ_PARITY_SETBITARRAY:
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_build_bit_array(input, parity_case);
      fuzz_parity_append_key_arg(parity_case, 0);
      parity_case->argv32[parity_case->argc32++] = parity_case->bit_array;
      parity_case->argv64[parity_case->argc64++] = parity_case->bit_array;
      return;
    case FUZZ_PARITY_GETBITARRAY:
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_append_key_arg(parity_case, 0);
      return;
    case FUZZ_PARITY_BITOP: {
      FuzzBitOpKind kind = fuzz_consume_bitop_kind(input);
      FuzzParityBitOpAliasMode alias_mode =
          (FuzzParityBitOpAliasMode)fuzz_consume_u32_in_range(input, 0, FUZZ_PARITY_ALIAS_REPEAT_SOURCE);
      size_t source_count = kind == FUZZ_BITOP_NOT ? 1 : (2 + fuzz_consume_size_in_range(input, 0, 1));
      size_t source_indices[3] = {1, 2, 3};
      bool has_last = kind == FUZZ_BITOP_NOT && fuzz_consume_bool(input);

      if (kind == FUZZ_BITOP_INVALID) {
        kind = FUZZ_BITOP_OR;
      }

      if (alias_mode == FUZZ_PARITY_ALIAS_DEST_EQ_SRC0 && source_count > 0) {
        source_indices[0] = 0;
      } else if (alias_mode == FUZZ_PARITY_ALIAS_REPEAT_SOURCE && source_count > 1) {
        source_indices[1] = source_indices[0];
      }

      if (fuzz_consume_bool(input)) {
        fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      }
      for (size_t i = 0; i < source_count; i++) {
        if (parity_case->seed_keys[source_indices[i]]) {
          continue;
        }
        fuzz_parity_fill_seed_values(input, parity_case, source_indices[i], true);
      }

      parity_case->argv32[parity_case->argc32++] = fuzz_bitop_kind_name(kind);
      parity_case->argv64[parity_case->argc64++] = parity_case->argv32[parity_case->argc32 - 1];
      fuzz_parity_append_key_arg(parity_case, 0);
      for (size_t i = 0; i < source_count; i++) {
        fuzz_parity_append_key_arg(parity_case, source_indices[i]);
      }
      if (kind == FUZZ_BITOP_NOT && has_last) {
        const char* arg =
            fuzz_parity_push_u32(parity_case, fuzz_consume_u32_in_range(input, 0, FUZZ_PARITY_SAFE_VALUE_MAX));
        parity_case->argv32[parity_case->argc32++] = arg;
        parity_case->argv64[parity_case->argc64++] = arg;
      }
      return;
    }
    case FUZZ_PARITY_BITPOS:
      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      fuzz_parity_append_key_arg(parity_case, 0);
      parity_case->argv32[parity_case->argc32++] = fuzz_consume_bool(input) ? "1" : "0";
      parity_case->argv64[parity_case->argc64++] = parity_case->argv32[parity_case->argc32 - 1];
      return;
    case FUZZ_PARITY_CONTAINS: {
      static const char* modes[] = {"ALL", "ALL_STRICT", "EQ"};
      bool same_keys = fuzz_consume_bool(input);
      size_t rhs_index = same_keys ? 0 : 1;

      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      if (!same_keys) {
        fuzz_parity_fill_seed_values(input, parity_case, 1, false);
      }

      fuzz_parity_append_key_arg(parity_case, 0);
      fuzz_parity_append_key_arg(parity_case, rhs_index);
      if (fuzz_consume_bool(input)) {
        const char* mode = modes[fuzz_consume_size_in_range(input, 0, 2)];
        parity_case->argv32[parity_case->argc32++] = mode;
        parity_case->argv64[parity_case->argc64++] = mode;
      }
      return;
    }
    case FUZZ_PARITY_JACCARD: {
      bool same_keys = fuzz_consume_bool(input);
      size_t rhs_index = same_keys ? 0 : 1;

      fuzz_parity_fill_seed_values(input, parity_case, 0, false);
      if (!same_keys) {
        fuzz_parity_fill_seed_values(input, parity_case, 1, false);
      }

      fuzz_parity_append_key_arg(parity_case, 0);
      fuzz_parity_append_key_arg(parity_case, rhs_index);
      return;
    }
  }

  fuzz_require(false);
}

static void fuzz_parity_execute_case(FuzzRedisServer* server, const FuzzParityCase* parity_case,
                                     redisReply** reply32, redisReply** reply64) {
  fuzz_require(reply32 != NULL);
  fuzz_require(reply64 != NULL);

  if (parity_case->spec->kind == FUZZ_PARITY_SETFULL) {
    /*
     * R64.SETFULL attempts to materialize the full 64-bit universe, which is
     * not practical under sanitizer-backed fuzzing time budgets. Model the
     * shared observable range instead so the parity oracle remains bounded.
     */
    *reply32 = fuzz_redis_command(server, "R.SETRANGE %s 0 256", parity_case->argv32[1]);
    *reply64 = fuzz_redis_command(server, "R64.SETRANGE %s 0 256", parity_case->argv64[1]);
    return;
  }

  *reply32 = fuzz_redis_command_argv(server, parity_case->argc32, parity_case->argv32);
  *reply64 = fuzz_redis_command_argv(server, parity_case->argc64, parity_case->argv64);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0) {
    return 0;
  }

  FuzzInput input;
  FuzzParityCase parity_case;
  redisReply* reply32;
  redisReply* reply64;
  FuzzRedisServer* server = fuzz_parity_server();
  const FuzzParityCommandSpec* spec;

  fuzz_input_init(&input, data, size);
  spec = &FUZZ_PARITY_COMMANDS[fuzz_consume_size_in_range(
      &input, 0, (sizeof(FUZZ_PARITY_COMMANDS) / sizeof(FUZZ_PARITY_COMMANDS[0])) - 1)];

  fuzz_parity_case_init(&parity_case, spec);
  fuzz_parity_build_case(&input, &parity_case);

  fuzz_redis_flushall(server);
  fuzz_parity_seed_case(server, &parity_case);

  fuzz_parity_execute_case(server, &parity_case, &reply32, &reply64);
  fuzz_parity_compare_reply(reply32, reply64);
  fuzz_redis_free_reply(reply32);
  fuzz_redis_free_reply(reply64);

  fuzz_parity_compare_case_state(server, &parity_case);

  redisReply* ping = fuzz_redis_command(server, "PING");
  fuzz_redis_expect_status(ping, "PONG");
  fuzz_redis_free_reply(ping);

  return 0;
}
