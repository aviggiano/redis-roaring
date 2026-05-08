#define _GNU_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuzz_bitop_oracles.h"
#include "fuzz_common.h"

#define FUZZ_PERSIST_KEYS 5
#define FUZZ_PERSIST_STEPS 3

typedef struct {
  size_t count;
  uint32_t* values32;
  uint64_t* values64;
} FuzzKeySnapshot;

typedef struct {
  FuzzBitOpKind kind;
  size_t dest_index;
  size_t source_count;
  size_t source_indices[3];
  bool has_last;
  uint64_t last_arg;
} FuzzPersistStep;

static const char* FUZZ_KEYS_32[FUZZ_PERSIST_KEYS] = {"r:src1", "r:src2", "r:src3", "r:tmp1", "r:tmp2"};
static const char* FUZZ_KEYS_64[FUZZ_PERSIST_KEYS] = {"r64:src1", "r64:src2", "r64:src3", "r64:tmp1", "r64:tmp2"};

static void fuzz_snapshot_free(FuzzKeySnapshot* snapshot, bool use_64) {
  if (use_64) {
    safe_free(snapshot->values64);
  } else {
    safe_free(snapshot->values32);
  }
  memset(snapshot, 0, sizeof(*snapshot));
}

static void fuzz_capture_snapshot(FuzzRedisServer* server, bool use_64,
                                  FuzzKeySnapshot snapshots[FUZZ_PERSIST_KEYS]) {
  for (size_t i = 0; i < FUZZ_PERSIST_KEYS; i++) {
    fuzz_snapshot_free(&snapshots[i], use_64);
    if (use_64) {
      snapshots[i].values64 = fuzz_redis_get_int_array64(server, FUZZ_KEYS_64[i], &snapshots[i].count);
    } else {
      snapshots[i].values32 = fuzz_redis_get_int_array32(server, FUZZ_KEYS_32[i], &snapshots[i].count);
    }
  }
}

static void fuzz_require_snapshot_matches(FuzzRedisServer* server, bool use_64,
                                          FuzzKeySnapshot snapshots[FUZZ_PERSIST_KEYS]) {
  for (size_t i = 0; i < FUZZ_PERSIST_KEYS; i++) {
    if (use_64) {
      size_t count = 0;
      uint64_t* values = fuzz_redis_get_int_array64(server, FUZZ_KEYS_64[i], &count);
      fuzz_require_uint64_arrays_equal(snapshots[i].values64, snapshots[i].count, values, count);
      safe_free(values);
    } else {
      size_t count = 0;
      uint32_t* values = fuzz_redis_get_int_array32(server, FUZZ_KEYS_32[i], &count);
      fuzz_require_uint32_arrays_equal(snapshots[i].values32, snapshots[i].count, values, count);
      safe_free(values);
    }
  }
}

static void fuzz_apply_steps(FuzzRedisServer* server, bool use_64,
                             const FuzzPersistStep steps[FUZZ_PERSIST_STEPS], size_t step_count) {
  const char* const* keys = use_64 ? FUZZ_KEYS_64 : FUZZ_KEYS_32;
  const char* command = use_64 ? "R64.BITOP" : "R.BITOP";

  for (size_t i = 0; i < step_count; i++) {
    const FuzzPersistStep* step = &steps[i];
    const char* argv[8];
    int argc = 0;
    argv[argc++] = command;
    argv[argc++] = fuzz_bitop_kind_name(step->kind);
    argv[argc++] = keys[step->dest_index];
    for (size_t j = 0; j < step->source_count; j++) {
      argv[argc++] = keys[step->source_indices[j]];
    }

    char last_buffer[32];
    if (step->kind == FUZZ_BITOP_NOT && step->has_last) {
      snprintf(last_buffer, sizeof(last_buffer), "%llu", (unsigned long long)step->last_arg);
      argv[argc++] = last_buffer;
    }

    redisReply* reply = fuzz_redis_command_argv(server, argc, argv);
    fuzz_reply_require(reply);
    fuzz_require(reply->type == REDIS_REPLY_INTEGER);
    fuzz_redis_free_reply(reply);
  }
}

static void fuzz_seed_sources(FuzzRedisServer* server, bool use_64,
                              size_t counts[3], uint32_t values32[3][16], uint64_t values64[3][16]) {
  const char* const* keys = use_64 ? FUZZ_KEYS_64 : FUZZ_KEYS_32;
  for (size_t i = 0; i < 3; i++) {
    if (use_64) {
      if (counts[i] > 0) {
        fuzz_redis_set_int_array64(server, "R64.SETINTARRAY", keys[i], counts[i], values64[i]);
      }
    } else if (counts[i] > 0) {
      fuzz_redis_set_int_array32(server, "R.SETINTARRAY", keys[i], counts[i], values32[i]);
    }
  }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 8) {
    return 0;
  }

  FuzzInput input;
  fuzz_input_init(&input, data, size);

  bool use_64 = fuzz_consume_bool(&input);
  size_t source_counts[3];
  uint32_t source_values32[3][16];
  uint64_t source_values64[3][16];
  FuzzPersistStep steps[FUZZ_PERSIST_STEPS];
  size_t step_count = 1 + fuzz_consume_size_in_range(&input, 0, FUZZ_PERSIST_STEPS - 1);

  for (size_t i = 0; i < 3; i++) {
    if (use_64) {
      source_counts[i] = fuzz_generate_values64(&input, source_values64[i], 16, 2048);
    } else {
      source_counts[i] = fuzz_generate_values32(&input, source_values32[i], 16, 2048);
    }
  }

  for (size_t i = 0; i < step_count; i++) {
    FuzzBitOpKind kind = fuzz_consume_bitop_kind(&input);
    while (kind == FUZZ_BITOP_INVALID) {
      kind = fuzz_consume_bitop_kind(&input);
    }

    steps[i].kind = kind;
    steps[i].dest_index = 3 + (i % 2);
    steps[i].source_count = kind == FUZZ_BITOP_NOT ? 1 : (2 + fuzz_consume_size_in_range(&input, 0, 1));
    steps[i].has_last = kind == FUZZ_BITOP_NOT && fuzz_consume_bool(&input);
    steps[i].last_arg = fuzz_consume_u64_in_range(&input, 0, 2048);

    size_t available = 3 + i;
    for (size_t j = 0; j < steps[i].source_count; j++) {
      steps[i].source_indices[j] = fuzz_consume_size_in_range(&input, 0, available - 1);
    }
  }

  FuzzRedisServer rdb_server;
  memset(&rdb_server, 0, sizeof(rdb_server));
  fuzz_require(fuzz_redis_start(&rdb_server, false, false, false));
  fuzz_seed_sources(&rdb_server, use_64, source_counts, source_values32, source_values64);
  fuzz_apply_steps(&rdb_server, use_64, steps, step_count);

  FuzzKeySnapshot rdb_snapshot[FUZZ_PERSIST_KEYS] = {0};
  fuzz_capture_snapshot(&rdb_server, use_64, rdb_snapshot);

  redisReply* reply = fuzz_redis_command(&rdb_server, "SAVE");
  fuzz_redis_expect_status(reply, "OK");
  fuzz_redis_free_reply(reply);

  fuzz_require(fuzz_redis_restart(&rdb_server, true));
  fuzz_require_snapshot_matches(&rdb_server, use_64, rdb_snapshot);
  fuzz_redis_shutdown(&rdb_server, false);

  FuzzRedisServer aof_server;
  memset(&aof_server, 0, sizeof(aof_server));
  fuzz_require(fuzz_redis_start(&aof_server, false, true, false));
  fuzz_seed_sources(&aof_server, use_64, source_counts, source_values32, source_values64);
  fuzz_apply_steps(&aof_server, use_64, steps, step_count);

  FuzzKeySnapshot aof_snapshot[FUZZ_PERSIST_KEYS] = {0};
  fuzz_capture_snapshot(&aof_server, use_64, aof_snapshot);

  reply = fuzz_redis_command(&aof_server, "BGREWRITEAOF");
  fuzz_reply_require(reply);
  fuzz_require(reply->type == REDIS_REPLY_STATUS || reply->type == REDIS_REPLY_STRING);
  fuzz_redis_free_reply(reply);
  fuzz_redis_wait_for_aof_rewrite(&aof_server);

  fuzz_require(fuzz_redis_restart(&aof_server, true));
  fuzz_require_snapshot_matches(&aof_server, use_64, aof_snapshot);
  fuzz_redis_shutdown(&aof_server, false);

  for (size_t i = 0; i < FUZZ_PERSIST_KEYS; i++) {
    fuzz_snapshot_free(&rdb_snapshot[i], use_64);
    fuzz_snapshot_free(&aof_snapshot[i], use_64);
  }

  return 0;
}
