#define _GNU_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuzz_bitop_oracles.h"
#include "fuzz_common.h"

typedef enum {
  FUZZ_PARITY_DISTINCT = 0,
  FUZZ_PARITY_DEST_EQ_SRC0,
  FUZZ_PARITY_REPEAT_SOURCE,
} FuzzParityAliasMode;

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

static void fuzz_seed_key_pair(FuzzRedisServer* server, const char* key32, const char* key64,
                               size_t count, const uint32_t* values32) {
  if (count == 0) {
    return;
  }

  uint64_t values64[16];
  for (size_t i = 0; i < count; i++) {
    values64[i] = values32[i];
  }

  fuzz_redis_set_int_array32(server, "R.SETINTARRAY", key32, count, values32);
  fuzz_redis_set_int_array64(server, "R64.SETINTARRAY", key64, count, values64);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 4) {
    return 0;
  }

  FuzzInput input;
  fuzz_input_init(&input, data, size);

  FuzzRedisServer* server = fuzz_parity_server();
  fuzz_redis_flushall(server);

  FuzzBitOpKind kind = fuzz_consume_bitop_kind(&input);
  while (kind == FUZZ_BITOP_INVALID) {
    kind = fuzz_consume_bitop_kind(&input);
  }

  FuzzParityAliasMode alias_mode = (FuzzParityAliasMode)fuzz_consume_u32_in_range(&input, 0, FUZZ_PARITY_REPEAT_SOURCE);
  size_t source_count = kind == FUZZ_BITOP_NOT ? 1 : (2 + fuzz_consume_size_in_range(&input, 0, 2));
  bool has_last = kind == FUZZ_BITOP_NOT && fuzz_consume_bool(&input);
  uint32_t last_arg = fuzz_consume_u32_in_range(&input, 0, 2048);

  const char* keys32[4] = {"r:dest", "r:src1", "r:src2", "r:src3"};
  const char* keys64[4] = {"r64:dest", "r64:src1", "r64:src2", "r64:src3"};
  const char* src_keys32[4] = {keys32[1], keys32[2], keys32[3], keys32[3]};
  const char* src_keys64[4] = {keys64[1], keys64[2], keys64[3], keys64[3]};

  if (alias_mode == FUZZ_PARITY_DEST_EQ_SRC0 && source_count > 0) {
    src_keys32[0] = keys32[0];
    src_keys64[0] = keys64[0];
  } else if (alias_mode == FUZZ_PARITY_REPEAT_SOURCE && source_count > 1) {
    src_keys32[1] = src_keys32[0];
    src_keys64[1] = src_keys64[0];
  }

  uint32_t value_sets[4][16];
  size_t value_counts[4] = {0};
  for (size_t i = 0; i < source_count; i++) {
    if (i > 0 && src_keys32[i] == src_keys32[0]) {
      value_counts[i] = value_counts[0];
      memcpy(value_sets[i], value_sets[0], value_counts[0] * sizeof(uint32_t));
      continue;
    }
    value_counts[i] = fuzz_generate_values32(&input, value_sets[i], 16, 2048);
  }

  for (size_t i = 0; i < source_count; i++) {
    bool already_seeded = false;
    for (size_t j = 0; j < i; j++) {
      if (strcmp(src_keys32[i], src_keys32[j]) == 0) {
        already_seeded = true;
        break;
      }
    }
    if (!already_seeded) {
      fuzz_seed_key_pair(server, src_keys32[i], src_keys64[i], value_counts[i], value_sets[i]);
    }
  }

  const char* argv32[12];
  const char* argv64[12];
  int argc32 = 0;
  int argc64 = 0;

  argv32[argc32++] = "R.BITOP";
  argv64[argc64++] = "R64.BITOP";
  argv32[argc32++] = fuzz_bitop_kind_name(kind);
  argv64[argc64++] = fuzz_bitop_kind_name(kind);
  argv32[argc32++] = keys32[0];
  argv64[argc64++] = keys64[0];
  for (size_t i = 0; i < source_count; i++) {
    argv32[argc32++] = src_keys32[i];
    argv64[argc64++] = src_keys64[i];
  }

  char last_buffer[32];
  if (kind == FUZZ_BITOP_NOT && has_last) {
    snprintf(last_buffer, sizeof(last_buffer), "%u", last_arg);
    argv32[argc32++] = last_buffer;
    argv64[argc64++] = last_buffer;
  }

  redisReply* reply32 = fuzz_redis_command_argv(server, argc32, argv32);
  redisReply* reply64 = fuzz_redis_command_argv(server, argc64, argv64);
  fuzz_reply_require(reply32);
  fuzz_reply_require(reply64);
  fuzz_require(reply32->type == REDIS_REPLY_INTEGER);
  fuzz_require(reply64->type == REDIS_REPLY_INTEGER);
  fuzz_require(reply32->integer == reply64->integer);
  fuzz_redis_free_reply(reply32);
  fuzz_redis_free_reply(reply64);

  size_t count32 = 0;
  size_t count64 = 0;
  uint32_t* values32 = fuzz_redis_get_int_array32(server, keys32[0], &count32);
  uint64_t* values64 = fuzz_redis_get_int_array64(server, keys64[0], &count64);
  fuzz_require(count32 == count64);
  for (size_t i = 0; i < count32; i++) {
    fuzz_require(values32[i] == values64[i]);
  }
  safe_free(values32);
  safe_free(values64);

  for (size_t i = 0; i < source_count; i++) {
    if (strcmp(src_keys32[i], keys32[0]) == 0) {
      continue;
    }

    count32 = 0;
    count64 = 0;
    values32 = fuzz_redis_get_int_array32(server, src_keys32[i], &count32);
    values64 = fuzz_redis_get_int_array64(server, src_keys64[i], &count64);
    fuzz_require(count32 == count64);
    for (size_t j = 0; j < count32; j++) {
      fuzz_require(values32[j] == values64[j]);
    }
    safe_free(values32);
    safe_free(values64);
  }

  redisReply* ping = fuzz_redis_command(server, "PING");
  fuzz_redis_expect_status(ping, "PONG");
  fuzz_redis_free_reply(ping);

  return 0;
}
