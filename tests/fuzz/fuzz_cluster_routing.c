#define _GNU_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuzz_bitop_oracles.h"
#include "fuzz_common.h"

static FuzzRedisServer FUZZ_CLUSTER_SERVER;
static bool FUZZ_CLUSTER_SERVER_READY = false;
static bool FUZZ_CLUSTER_FUNCTIONS_READY = false;
static bool FUZZ_CLUSTER_FUNCTIONS_SUPPORTED = false;

static const char* FUZZ_CLUSTER_FUNCTION_LIBRARY =
    "#!lua name=rrfuzz\n"
    "redis.register_function('rr_bitop32_or', function(keys, args)\n"
    "  return redis.call('R.BITOP', 'OR', keys[1], keys[2], keys[3])\n"
    "end)\n"
    "redis.register_function('rr_bitop32_not', function(keys, args)\n"
    "  if #args > 0 then\n"
    "    return redis.call('R.BITOP', 'NOT', keys[1], keys[2], args[1])\n"
    "  end\n"
    "  return redis.call('R.BITOP', 'NOT', keys[1], keys[2])\n"
    "end)\n"
    "redis.register_function('rr_bitop64_or', function(keys, args)\n"
    "  return redis.call('R64.BITOP', 'OR', keys[1], keys[2], keys[3])\n"
    "end)\n"
    "redis.register_function('rr_bitop64_not', function(keys, args)\n"
    "  if #args > 0 then\n"
    "    return redis.call('R64.BITOP', 'NOT', keys[1], keys[2], args[1])\n"
    "  end\n"
    "  return redis.call('R64.BITOP', 'NOT', keys[1], keys[2])\n"
    "end)\n";

static void fuzz_cluster_cleanup(void) {
  if (FUZZ_CLUSTER_SERVER_READY) {
    fuzz_redis_shutdown(&FUZZ_CLUSTER_SERVER, false);
    FUZZ_CLUSTER_SERVER_READY = false;
  }
  FUZZ_CLUSTER_FUNCTIONS_READY = false;
  FUZZ_CLUSTER_FUNCTIONS_SUPPORTED = false;
}

static void fuzz_cluster_load_functions(FuzzRedisServer* server) {
  if (FUZZ_CLUSTER_FUNCTIONS_READY) {
    return;
  }

  const char* argv[] = {"FUNCTION", "LOAD", "REPLACE", FUZZ_CLUSTER_FUNCTION_LIBRARY};
  redisReply* reply = fuzz_redis_command_argv(server, 4, argv);
  fuzz_reply_require(reply);

  if (reply->type == REDIS_REPLY_ERROR) {
    fuzz_require(strstr(reply->str, "unknown command") != NULL);
    FUZZ_CLUSTER_FUNCTIONS_SUPPORTED = false;
  } else {
    fuzz_require(reply->type == REDIS_REPLY_STATUS || reply->type == REDIS_REPLY_STRING);
    fuzz_require(strcmp(reply->str, "rrfuzz") == 0);
    FUZZ_CLUSTER_FUNCTIONS_SUPPORTED = true;
  }

  fuzz_redis_free_reply(reply);
  FUZZ_CLUSTER_FUNCTIONS_READY = true;
}

static FuzzRedisServer* fuzz_cluster_server(void) {
  if (!FUZZ_CLUSTER_SERVER_READY) {
    fuzz_require(fuzz_redis_start(&FUZZ_CLUSTER_SERVER, true, false, false));
    fuzz_cluster_load_functions(&FUZZ_CLUSTER_SERVER);
    atexit(fuzz_cluster_cleanup);
    FUZZ_CLUSTER_SERVER_READY = true;
  }

  return &FUZZ_CLUSTER_SERVER;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 5) {
    return 0;
  }

  FuzzInput input;
  fuzz_input_init(&input, data, size);

  FuzzRedisServer* server = fuzz_cluster_server();
  fuzz_redis_flushall(server);

  bool use_64 = fuzz_consume_bool(&input);
  bool cross_slot = fuzz_consume_bool(&input);
  FuzzBitOpKind kind = fuzz_consume_bool(&input) ? FUZZ_BITOP_NOT : FUZZ_BITOP_OR;
  bool via_function = fuzz_consume_bool(&input) && FUZZ_CLUSTER_FUNCTIONS_SUPPORTED;
  bool has_last = kind == FUZZ_BITOP_NOT && fuzz_consume_bool(&input);

  const char* dest = cross_slot ? "{dest}dest" : "{bitop}dest";
  const char* src1 = cross_slot ? "{src1}foo" : "{bitop}foo";
  const char* src2 = cross_slot ? "{src2}bar" : "{bitop}bar";

  if (use_64) {
    uint64_t dest_values[16];
    uint64_t values1[16];
    uint64_t values2[16];
    size_t dest_count = fuzz_generate_values64(&input, dest_values, 16, 2048);
    size_t count1 = fuzz_generate_values64(&input, values1, 16, 2048);
    size_t count2 = fuzz_generate_values64(&input, values2, 16, 2048);
    if (kind == FUZZ_BITOP_NOT) {
      count2 = 0;
    }

    Bitmap64* initial_dest = fuzz_bitmap64_from_values(dest_count, dest_values);
    Bitmap64* source1 = fuzz_bitmap64_from_values(count1, values1);
    Bitmap64* source2 = fuzz_bitmap64_from_values(count2, values2);
    fuzz_seed_key64(server, dest, dest_count, dest_values);
    fuzz_seed_key64(server, src1, count1, values1);
    if (kind != FUZZ_BITOP_NOT) {
      fuzz_seed_key64(server, src2, count2, values2);
    }

    const char* argv[8];
    const char* function_name = kind == FUZZ_BITOP_NOT ? "rr_bitop64_not" : "rr_bitop64_or";
    int argc = 0;
    if (via_function) {
      argv[argc++] = "FCALL";
      argv[argc++] = function_name;
      argv[argc++] = kind == FUZZ_BITOP_NOT ? "2" : "3";
      argv[argc++] = dest;
      argv[argc++] = src1;
      if (kind != FUZZ_BITOP_NOT) {
        argv[argc++] = src2;
      }
    } else {
      argv[argc++] = "R64.BITOP";
      argv[argc++] = fuzz_bitop_kind_name(kind);
      argv[argc++] = dest;
      argv[argc++] = src1;
      if (kind != FUZZ_BITOP_NOT) {
        argv[argc++] = src2;
      }
    }

    char last_buffer[32];
    uint64_t last_arg = fuzz_consume_u64_in_range(&input, 0, 2048);
    if (kind == FUZZ_BITOP_NOT && has_last) {
      snprintf(last_buffer, sizeof(last_buffer), "%llu", (unsigned long long)last_arg);
      argv[argc++] = last_buffer;
    }

    redisReply* reply = fuzz_redis_command_argv(server, argc, argv);
    if (cross_slot) {
      fuzz_reply_require(reply);
      fuzz_require(reply->type == REDIS_REPLY_ERROR);
      fuzz_require(strstr(reply->str, "CROSSSLOT") != NULL);
      fuzz_redis_free_reply(reply);
      fuzz_require_bitmap64_matches_key(server, dest, initial_dest);
      fuzz_require_bitmap64_matches_key(server, src1, source1);
      if (kind != FUZZ_BITOP_NOT) {
        fuzz_require_bitmap64_matches_key(server, src2, source2);
      }
    } else {
      const Bitmap64* sources[2] = {source1, source2};
      Bitmap64* expected = fuzz_bitop_expected64(kind, kind == FUZZ_BITOP_NOT ? 1 : 2, sources, has_last, last_arg);
      fuzz_require(reply->type == REDIS_REPLY_INTEGER);
      fuzz_require((uint64_t)reply->integer == bitmap64_get_cardinality(expected));
      fuzz_redis_free_reply(reply);
      fuzz_require_bitmap64_matches_key(server, dest, expected);
      fuzz_require_bitmap64_matches_key(server, src1, source1);
      if (kind != FUZZ_BITOP_NOT) {
        fuzz_require_bitmap64_matches_key(server, src2, source2);
      }
      bitmap64_free(expected);
    }

    bitmap64_free(initial_dest);
    bitmap64_free(source1);
    bitmap64_free(source2);
  } else {
    uint32_t dest_values[16];
    uint32_t values1[16];
    uint32_t values2[16];
    size_t dest_count = fuzz_generate_values32(&input, dest_values, 16, 2048);
    size_t count1 = fuzz_generate_values32(&input, values1, 16, 2048);
    size_t count2 = fuzz_generate_values32(&input, values2, 16, 2048);
    if (kind == FUZZ_BITOP_NOT) {
      count2 = 0;
    }

    Bitmap* initial_dest = fuzz_bitmap32_from_values(dest_count, dest_values);
    Bitmap* source1 = fuzz_bitmap32_from_values(count1, values1);
    Bitmap* source2 = fuzz_bitmap32_from_values(count2, values2);
    fuzz_seed_key32(server, dest, dest_count, dest_values);
    fuzz_seed_key32(server, src1, count1, values1);
    if (kind != FUZZ_BITOP_NOT) {
      fuzz_seed_key32(server, src2, count2, values2);
    }

    const char* argv[8];
    const char* function_name = kind == FUZZ_BITOP_NOT ? "rr_bitop32_not" : "rr_bitop32_or";
    int argc = 0;
    if (via_function) {
      argv[argc++] = "FCALL";
      argv[argc++] = function_name;
      argv[argc++] = kind == FUZZ_BITOP_NOT ? "2" : "3";
      argv[argc++] = dest;
      argv[argc++] = src1;
      if (kind != FUZZ_BITOP_NOT) {
        argv[argc++] = src2;
      }
    } else {
      argv[argc++] = "R.BITOP";
      argv[argc++] = fuzz_bitop_kind_name(kind);
      argv[argc++] = dest;
      argv[argc++] = src1;
      if (kind != FUZZ_BITOP_NOT) {
        argv[argc++] = src2;
      }
    }

    char last_buffer[32];
    uint32_t last_arg = fuzz_consume_u32_in_range(&input, 0, 2048);
    if (kind == FUZZ_BITOP_NOT && has_last) {
      snprintf(last_buffer, sizeof(last_buffer), "%u", last_arg);
      argv[argc++] = last_buffer;
    }

    redisReply* reply = fuzz_redis_command_argv(server, argc, argv);
    if (cross_slot) {
      fuzz_reply_require(reply);
      fuzz_require(reply->type == REDIS_REPLY_ERROR);
      fuzz_require(strstr(reply->str, "CROSSSLOT") != NULL);
      fuzz_redis_free_reply(reply);
      fuzz_require_bitmap32_matches_key(server, dest, initial_dest);
      fuzz_require_bitmap32_matches_key(server, src1, source1);
      if (kind != FUZZ_BITOP_NOT) {
        fuzz_require_bitmap32_matches_key(server, src2, source2);
      }
    } else {
      const Bitmap* sources[2] = {source1, source2};
      Bitmap* expected = fuzz_bitop_expected32(kind, kind == FUZZ_BITOP_NOT ? 1 : 2, sources, has_last, last_arg);
      fuzz_require(reply->type == REDIS_REPLY_INTEGER);
      fuzz_require((uint64_t)reply->integer == bitmap_get_cardinality(expected));
      fuzz_redis_free_reply(reply);
      fuzz_require_bitmap32_matches_key(server, dest, expected);
      fuzz_require_bitmap32_matches_key(server, src1, source1);
      if (kind != FUZZ_BITOP_NOT) {
        fuzz_require_bitmap32_matches_key(server, src2, source2);
      }
      bitmap_free(expected);
    }

    bitmap_free(initial_dest);
    bitmap_free(source1);
    bitmap_free(source2);
  }

  redisReply* ping = fuzz_redis_command(server, "PING");
  fuzz_redis_expect_status(ping, "PONG");
  fuzz_redis_free_reply(ping);

  return 0;
}
