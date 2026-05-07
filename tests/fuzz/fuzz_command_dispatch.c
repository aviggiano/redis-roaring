#define _GNU_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuzz_bitop_oracles.h"
#include "fuzz_common.h"

typedef enum {
  FUZZ_ALIAS_DISTINCT = 0,
  FUZZ_ALIAS_DEST_EQ_SRC0,
  FUZZ_ALIAS_REPEAT_FIRST_SOURCE,
} FuzzAliasMode;

static FuzzRedisServer FUZZ_DISPATCH_SERVER;
static bool FUZZ_DISPATCH_SERVER_READY = false;

static void fuzz_dispatch_cleanup(void) {
  if (FUZZ_DISPATCH_SERVER_READY) {
    fuzz_redis_shutdown(&FUZZ_DISPATCH_SERVER, false);
    FUZZ_DISPATCH_SERVER_READY = false;
  }
}

static FuzzRedisServer* fuzz_dispatch_server(void) {
  if (!FUZZ_DISPATCH_SERVER_READY) {
    fuzz_require(fuzz_redis_start(&FUZZ_DISPATCH_SERVER, false, false, false));
    atexit(fuzz_dispatch_cleanup);
    FUZZ_DISPATCH_SERVER_READY = true;
  }

  return &FUZZ_DISPATCH_SERVER;
}

static void fuzz_dispatch_require_error(redisReply* reply) {
  fuzz_reply_require(reply);
  fuzz_require(reply->type == REDIS_REPLY_ERROR);
}

static bool fuzz_dispatch_find_previous_source(size_t index, const char* const* source_keys, size_t* match_index) {
  for (size_t i = 0; i < index; i++) {
    if (strcmp(source_keys[index], source_keys[i]) == 0) {
      *match_index = i;
      return true;
    }
  }

  return false;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 4) {
    return 0;
  }

  FuzzInput input;
  fuzz_input_init(&input, data, size);

  FuzzRedisServer* server = fuzz_dispatch_server();
  fuzz_redis_flushall(server);

  bool use_64 = fuzz_consume_bool(&input);
  FuzzBitOpKind kind = fuzz_consume_bitop_kind(&input);
  bool invalid_arity = fuzz_consume_bool(&input);
  FuzzAliasMode alias_mode = (FuzzAliasMode)fuzz_consume_u32_in_range(&input, 0, FUZZ_ALIAS_REPEAT_FIRST_SOURCE);

  size_t source_count = kind == FUZZ_BITOP_NOT ? 1 : (2 + fuzz_consume_size_in_range(&input, 0, 2));
  bool has_last = kind == FUZZ_BITOP_NOT && fuzz_consume_bool(&input);

  const char* key_names[4] = {"dest", "src1", "src2", "src3"};
  const char* source_keys[4] = {key_names[1], key_names[2], key_names[3], key_names[3]};

  if (alias_mode == FUZZ_ALIAS_DEST_EQ_SRC0 && source_count > 0) {
    source_keys[0] = key_names[0];
  } else if (alias_mode == FUZZ_ALIAS_REPEAT_FIRST_SOURCE && source_count > 1) {
    source_keys[1] = source_keys[0];
  }

  if (!fuzz_bitop_kind_is_variadic(kind) && kind != FUZZ_BITOP_NOT) {
    invalid_arity = false;
  }

  if (use_64) {
    uint64_t dest_values[16];
    uint64_t source_values[4][16];
    size_t dest_count = fuzz_generate_values64(&input, dest_values, 16, 2048);
    size_t source_value_counts[4] = {0};
    Bitmap64* logical_sources[4] = {NULL};
    Bitmap64* logical_dest = NULL;

    for (size_t i = 0; i < source_count; i++) {
      size_t previous_index = 0;
      if (fuzz_dispatch_find_previous_source(i, source_keys, &previous_index)) {
        logical_sources[i] = roaring64_bitmap_copy(logical_sources[previous_index]);
        source_value_counts[i] = source_value_counts[previous_index];
        continue;
      }
      source_value_counts[i] = fuzz_generate_values64(&input, source_values[i], 16, 2048);
      logical_sources[i] = fuzz_bitmap64_from_values(source_value_counts[i], source_values[i]);
    }

    if (source_keys[0] == key_names[0]) {
      fuzz_seed_key64(server, key_names[0], source_value_counts[0], source_values[0]);
      logical_dest = roaring64_bitmap_copy(logical_sources[0]);
    } else {
      fuzz_seed_key64(server, key_names[0], dest_count, dest_values);
      logical_dest = fuzz_bitmap64_from_values(dest_count, dest_values);
    }
    fuzz_require(logical_dest != NULL);

    for (size_t i = 0; i < source_count; i++) {
      bool already_seeded = false;
      for (size_t j = 0; j < i; j++) {
        if (strcmp(source_keys[i], source_keys[j]) == 0) {
          already_seeded = true;
          break;
        }
      }
      if (!already_seeded && strcmp(source_keys[i], key_names[0]) != 0) {
        fuzz_seed_key64(server, source_keys[i], source_value_counts[i], source_values[i]);
      }
    }

    const char* argv[12];
    int argc = 0;
    argv[argc++] = "R64.BITOP";
    argv[argc++] = fuzz_bitop_kind_name(kind);
    argv[argc++] = key_names[0];
    for (size_t i = 0; i < source_count; i++) {
      argv[argc++] = source_keys[i];
    }

    char last_buffer[32];
    uint64_t last_arg = fuzz_consume_u64_in_range(&input, 0, 2048);
    if (kind == FUZZ_BITOP_NOT && has_last) {
      snprintf(last_buffer, sizeof(last_buffer), "%llu", (unsigned long long)last_arg);
      argv[argc++] = last_buffer;
    }

    if (invalid_arity) {
      if (kind == FUZZ_BITOP_NOT) {
        argc = 3;
      } else {
        argc = 4;
      }
    }

    redisReply* reply = fuzz_redis_command_argv(server, argc, argv);
    if (kind == FUZZ_BITOP_INVALID || invalid_arity) {
      fuzz_dispatch_require_error(reply);
      fuzz_redis_free_reply(reply);
      fuzz_require_bitmap64_matches_key(server, key_names[0], logical_dest);
      for (size_t i = 0; i < source_count; i++) {
        size_t previous_index = 0;
        if (strcmp(source_keys[i], key_names[0]) != 0
            && !fuzz_dispatch_find_previous_source(i, source_keys, &previous_index)) {
          fuzz_require_bitmap64_matches_key(server, source_keys[i], logical_sources[i]);
        }
      }
    } else {
      const Bitmap64** oracle_sources = malloc(source_count * sizeof(*oracle_sources));
      fuzz_require(oracle_sources != NULL);
      for (size_t i = 0; i < source_count; i++) {
        oracle_sources[i] = logical_sources[i];
      }

      Bitmap64* expected = fuzz_bitop_expected64(kind, source_count, oracle_sources, has_last, last_arg);
      fuzz_require(expected != NULL);
      fuzz_require(reply->type == REDIS_REPLY_INTEGER);
      fuzz_require((uint64_t)reply->integer == bitmap64_get_cardinality(expected));
      fuzz_redis_free_reply(reply);

      fuzz_require_bitmap64_matches_key(server, key_names[0], expected);

      for (size_t i = 0; i < source_count; i++) {
        size_t previous_index = 0;
        if (strcmp(source_keys[i], key_names[0]) != 0
            && !fuzz_dispatch_find_previous_source(i, source_keys, &previous_index)) {
          fuzz_require_bitmap64_matches_key(server, source_keys[i], logical_sources[i]);
        }
      }

      free(oracle_sources);
      bitmap64_free(expected);
    }

    bitmap64_free(logical_dest);
    for (size_t i = 0; i < source_count; i++) {
      if (logical_sources[i] != NULL) {
        bitmap64_free(logical_sources[i]);
      }
    }
  } else {
    uint32_t dest_values[16];
    uint32_t source_values[4][16];
    size_t dest_count = fuzz_generate_values32(&input, dest_values, 16, 2048);
    size_t source_value_counts[4] = {0};
    Bitmap* logical_sources[4] = {NULL};
    Bitmap* logical_dest = NULL;

    for (size_t i = 0; i < source_count; i++) {
      size_t previous_index = 0;
      if (fuzz_dispatch_find_previous_source(i, source_keys, &previous_index)) {
        logical_sources[i] = roaring_bitmap_copy(logical_sources[previous_index]);
        source_value_counts[i] = source_value_counts[previous_index];
        continue;
      }
      source_value_counts[i] = fuzz_generate_values32(&input, source_values[i], 16, 2048);
      logical_sources[i] = fuzz_bitmap32_from_values(source_value_counts[i], source_values[i]);
    }

    if (source_keys[0] == key_names[0]) {
      fuzz_seed_key32(server, key_names[0], source_value_counts[0], source_values[0]);
      logical_dest = roaring_bitmap_copy(logical_sources[0]);
    } else {
      fuzz_seed_key32(server, key_names[0], dest_count, dest_values);
      logical_dest = fuzz_bitmap32_from_values(dest_count, dest_values);
    }
    fuzz_require(logical_dest != NULL);

    for (size_t i = 0; i < source_count; i++) {
      bool already_seeded = false;
      for (size_t j = 0; j < i; j++) {
        if (strcmp(source_keys[i], source_keys[j]) == 0) {
          already_seeded = true;
          break;
        }
      }
      if (!already_seeded && strcmp(source_keys[i], key_names[0]) != 0) {
        fuzz_seed_key32(server, source_keys[i], source_value_counts[i], source_values[i]);
      }
    }

    const char* argv[12];
    int argc = 0;
    argv[argc++] = "R.BITOP";
    argv[argc++] = fuzz_bitop_kind_name(kind);
    argv[argc++] = key_names[0];
    for (size_t i = 0; i < source_count; i++) {
      argv[argc++] = source_keys[i];
    }

    char last_buffer[32];
    uint32_t last_arg = fuzz_consume_u32_in_range(&input, 0, 2048);
    if (kind == FUZZ_BITOP_NOT && has_last) {
      snprintf(last_buffer, sizeof(last_buffer), "%u", last_arg);
      argv[argc++] = last_buffer;
    }

    if (invalid_arity) {
      if (kind == FUZZ_BITOP_NOT) {
        argc = 3;
      } else {
        argc = 4;
      }
    }

    redisReply* reply = fuzz_redis_command_argv(server, argc, argv);
    if (kind == FUZZ_BITOP_INVALID || invalid_arity) {
      fuzz_dispatch_require_error(reply);
      fuzz_redis_free_reply(reply);
      fuzz_require_bitmap32_matches_key(server, key_names[0], logical_dest);
      for (size_t i = 0; i < source_count; i++) {
        size_t previous_index = 0;
        if (strcmp(source_keys[i], key_names[0]) != 0
            && !fuzz_dispatch_find_previous_source(i, source_keys, &previous_index)) {
          fuzz_require_bitmap32_matches_key(server, source_keys[i], logical_sources[i]);
        }
      }
    } else {
      const Bitmap** oracle_sources = malloc(source_count * sizeof(*oracle_sources));
      fuzz_require(oracle_sources != NULL);
      for (size_t i = 0; i < source_count; i++) {
        oracle_sources[i] = logical_sources[i];
      }

      Bitmap* expected = fuzz_bitop_expected32(kind, source_count, oracle_sources, has_last, last_arg);
      fuzz_require(expected != NULL);
      fuzz_require(reply->type == REDIS_REPLY_INTEGER);
      fuzz_require((uint64_t)reply->integer == bitmap_get_cardinality(expected));
      fuzz_redis_free_reply(reply);

      fuzz_require_bitmap32_matches_key(server, key_names[0], expected);

      for (size_t i = 0; i < source_count; i++) {
        size_t previous_index = 0;
        if (strcmp(source_keys[i], key_names[0]) != 0
            && !fuzz_dispatch_find_previous_source(i, source_keys, &previous_index)) {
          fuzz_require_bitmap32_matches_key(server, source_keys[i], logical_sources[i]);
        }
      }

      free(oracle_sources);
      bitmap_free(expected);
    }

    bitmap_free(logical_dest);
    for (size_t i = 0; i < source_count; i++) {
      if (logical_sources[i] != NULL) {
        bitmap_free(logical_sources[i]);
      }
    }
  }

  redisReply* ping = fuzz_redis_command(server, "PING");
  fuzz_redis_expect_status(ping, "PONG");
  fuzz_redis_free_reply(ping);

  return 0;
}
