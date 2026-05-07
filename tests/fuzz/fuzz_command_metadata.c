#define _GNU_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuzz_common.h"
#include "fuzz_redis_harness.h"

typedef enum {
  FUZZ_META_SINGLE_KEY_ONE = 0,
  FUZZ_META_SINGLE_KEY_TWO,
  FUZZ_META_SINGLE_KEY_THREE,
  FUZZ_META_SINGLE_KEY_VARIADIC,
  FUZZ_META_SINGLE_KEY_OPTIONAL,
  FUZZ_META_PAIR_KEYS,
  FUZZ_META_PAIR_KEYS_OPTIONAL,
  FUZZ_META_DEST_AND_SOURCES,
  FUZZ_META_BITOP_VARIADIC,
  FUZZ_META_BITOP_NOT,
} FuzzMetadataKind;

typedef enum {
  FUZZ_KEYFLAG_RO = 1U << 0,
  FUZZ_KEYFLAG_RW = 1U << 1,
  FUZZ_KEYFLAG_OW = 1U << 2,
  FUZZ_KEYFLAG_INSERT = 1U << 3,
  FUZZ_KEYFLAG_UPDATE = 1U << 4,
  FUZZ_KEYFLAG_ACCESS = 1U << 5,
  FUZZ_KEYFLAG_DELETE = 1U << 6,
} FuzzKeyFlagBits;

typedef uint32_t FuzzKeyFlags;

#define FUZZ_FLAGS_RO_ACCESS ((FuzzKeyFlags)(FUZZ_KEYFLAG_RO | FUZZ_KEYFLAG_ACCESS))
#define FUZZ_FLAGS_RW_UPDATE ((FuzzKeyFlags)(FUZZ_KEYFLAG_RW | FUZZ_KEYFLAG_UPDATE))
#define FUZZ_FLAGS_RO_INSERT ((FuzzKeyFlags)(FUZZ_KEYFLAG_RO | FUZZ_KEYFLAG_INSERT))
#define FUZZ_FLAGS_RW_INSERT ((FuzzKeyFlags)(FUZZ_KEYFLAG_RW | FUZZ_KEYFLAG_INSERT))
#define FUZZ_FLAGS_RW_DELETE ((FuzzKeyFlags)(FUZZ_KEYFLAG_RW | FUZZ_KEYFLAG_DELETE))
#define FUZZ_FLAGS_OW_INSERT ((FuzzKeyFlags)(FUZZ_KEYFLAG_OW | FUZZ_KEYFLAG_INSERT))
#define FUZZ_FLAGS_OW_DELETE ((FuzzKeyFlags)(FUZZ_KEYFLAG_OW | FUZZ_KEYFLAG_DELETE))

typedef struct {
  const char* command;
  FuzzMetadataKind kind;
  const char* optional_token;
  FuzzKeyFlags primary_flags;
  FuzzKeyFlags secondary_flags;
} FuzzMetadataSpec;

static const FuzzMetadataSpec FUZZ_METADATA_SPECS[] = {
    {"R.SETBIT", FUZZ_META_SINGLE_KEY_THREE, NULL, FUZZ_FLAGS_RW_UPDATE, 0},
    {"R.GETBIT", FUZZ_META_SINGLE_KEY_TWO, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R.GETBITS", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R.CLEARBITS", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RW_UPDATE, 0},
    {"R.SETINTARRAY", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RO_INSERT, 0},
    {"R.GETINTARRAY", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R.RANGEINTARRAY", FUZZ_META_SINGLE_KEY_THREE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R.APPENDINTARRAY", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RW_INSERT, 0},
    {"R.DELETEINTARRAY", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RW_DELETE, 0},
    {"R.DIFF", FUZZ_META_DEST_AND_SOURCES, NULL, FUZZ_FLAGS_OW_INSERT, FUZZ_FLAGS_RO_ACCESS},
    {"R.SETFULL", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_OW_INSERT, 0},
    {"R.SETRANGE", FUZZ_META_SINGLE_KEY_THREE, NULL, FUZZ_FLAGS_RW_UPDATE, 0},
    {"R.OPTIMIZE", FUZZ_META_SINGLE_KEY_OPTIONAL, "MEM", FUZZ_FLAGS_RW_UPDATE, 0},
    {"R.SETBITARRAY", FUZZ_META_SINGLE_KEY_TWO, NULL, FUZZ_FLAGS_RO_INSERT, 0},
    {"R.GETBITARRAY", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R.BITOP", FUZZ_META_BITOP_VARIADIC, NULL, FUZZ_FLAGS_RW_INSERT, FUZZ_FLAGS_RO_ACCESS},
    {"R.BITOP", FUZZ_META_BITOP_NOT, NULL, FUZZ_FLAGS_RW_INSERT, FUZZ_FLAGS_RO_ACCESS},
    {"R.BITCOUNT", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R.BITPOS", FUZZ_META_SINGLE_KEY_TWO, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R.MIN", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R.MAX", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R.CLEAR", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_OW_DELETE, 0},
    {"R.CONTAINS", FUZZ_META_PAIR_KEYS_OPTIONAL, "EQ", FUZZ_FLAGS_RO_ACCESS, FUZZ_FLAGS_RO_ACCESS},
    {"R.JACCARD", FUZZ_META_PAIR_KEYS, NULL, FUZZ_FLAGS_RO_ACCESS, FUZZ_FLAGS_RO_ACCESS},
    {"R64.SETBIT", FUZZ_META_SINGLE_KEY_THREE, NULL, FUZZ_FLAGS_RW_UPDATE, 0},
    {"R64.GETBIT", FUZZ_META_SINGLE_KEY_TWO, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R64.GETBITS", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R64.CLEARBITS", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RW_UPDATE, 0},
    {"R64.SETINTARRAY", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RO_INSERT, 0},
    {"R64.GETINTARRAY", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R64.RANGEINTARRAY", FUZZ_META_SINGLE_KEY_THREE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R64.APPENDINTARRAY", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RW_INSERT, 0},
    {"R64.DELETEINTARRAY", FUZZ_META_SINGLE_KEY_VARIADIC, NULL, FUZZ_FLAGS_RW_DELETE, 0},
    {"R64.DIFF", FUZZ_META_DEST_AND_SOURCES, NULL, FUZZ_FLAGS_OW_INSERT, FUZZ_FLAGS_RO_ACCESS},
    {"R64.SETFULL", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_OW_INSERT, 0},
    {"R64.SETRANGE", FUZZ_META_SINGLE_KEY_THREE, NULL, FUZZ_FLAGS_RW_UPDATE, 0},
    {"R64.OPTIMIZE", FUZZ_META_SINGLE_KEY_OPTIONAL, "MEM", FUZZ_FLAGS_RW_UPDATE, 0},
    {"R64.SETBITARRAY", FUZZ_META_SINGLE_KEY_TWO, NULL, FUZZ_FLAGS_RO_INSERT, 0},
    {"R64.GETBITARRAY", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R64.BITOP", FUZZ_META_BITOP_VARIADIC, NULL, FUZZ_FLAGS_RW_INSERT, FUZZ_FLAGS_RO_ACCESS},
    {"R64.BITOP", FUZZ_META_BITOP_NOT, NULL, FUZZ_FLAGS_RW_INSERT, FUZZ_FLAGS_RO_ACCESS},
    {"R64.BITCOUNT", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R64.BITPOS", FUZZ_META_SINGLE_KEY_TWO, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R64.MIN", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R64.MAX", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_RO_ACCESS, 0},
    {"R64.CLEAR", FUZZ_META_SINGLE_KEY_ONE, NULL, FUZZ_FLAGS_OW_DELETE, 0},
    {"R64.CONTAINS", FUZZ_META_PAIR_KEYS_OPTIONAL, "EQ", FUZZ_FLAGS_RO_ACCESS, FUZZ_FLAGS_RO_ACCESS},
    {"R64.JACCARD", FUZZ_META_PAIR_KEYS, NULL, FUZZ_FLAGS_RO_ACCESS, FUZZ_FLAGS_RO_ACCESS},
    {"R.STAT", FUZZ_META_SINGLE_KEY_OPTIONAL, "JSON", FUZZ_FLAGS_RO_ACCESS, 0},
};

static const char* FUZZ_BITOP_VARIADIC_OPS[] = {
    "AND", "OR", "XOR", "ANDOR", "ONE", "DIFF", "DIFF1"
};

static FuzzRedisServer FUZZ_METADATA_SERVER;
static bool FUZZ_METADATA_SERVER_READY = false;

static void fuzz_metadata_cleanup(void) {
  if (FUZZ_METADATA_SERVER_READY) {
    fuzz_redis_shutdown(&FUZZ_METADATA_SERVER, false);
    FUZZ_METADATA_SERVER_READY = false;
  }
}

static FuzzRedisServer* fuzz_metadata_server(void) {
  if (!FUZZ_METADATA_SERVER_READY) {
    fuzz_require(fuzz_redis_start(&FUZZ_METADATA_SERVER, false, false, false));
    atexit(fuzz_metadata_cleanup);
    FUZZ_METADATA_SERVER_READY = true;
  }

  return &FUZZ_METADATA_SERVER;
}

static int fuzz_metadata_build_valid_argv(const FuzzMetadataSpec* spec, FuzzInput* input,
                                          const char** argv, bool* expect_runtime_error) {
  int argc = 0;
  argv[argc++] = spec->command;
  *expect_runtime_error = false;

  switch (spec->kind) {
    case FUZZ_META_SINGLE_KEY_ONE:
      argv[argc++] = "key1";
      break;
    case FUZZ_META_SINGLE_KEY_TWO:
      argv[argc++] = "key1";
      argv[argc++] = "1";
      break;
    case FUZZ_META_SINGLE_KEY_THREE:
      argv[argc++] = "key1";
      argv[argc++] = "1";
      argv[argc++] = "2";
      break;
    case FUZZ_META_SINGLE_KEY_VARIADIC: {
      size_t extras = 1 + fuzz_consume_size_in_range(input, 0, 3);
      argv[argc++] = "key1";
      for (size_t i = 0; i < extras; i++) {
        argv[argc++] = (i & 1U) == 0U ? "1" : "2";
      }
      break;
    }
    case FUZZ_META_SINGLE_KEY_OPTIONAL:
      argv[argc++] = "key1";
      if (fuzz_consume_bool(input)) {
        argv[argc++] = spec->optional_token;
      }
      break;
    case FUZZ_META_PAIR_KEYS:
      argv[argc++] = "key1";
      argv[argc++] = "key2";
      break;
    case FUZZ_META_PAIR_KEYS_OPTIONAL:
      argv[argc++] = "key1";
      argv[argc++] = "key2";
      if (fuzz_consume_bool(input)) {
        argv[argc++] = spec->optional_token;
      }
      break;
    case FUZZ_META_DEST_AND_SOURCES:
      argv[argc++] = "dest";
      argv[argc++] = "src1";
      argv[argc++] = "src2";
      break;
    case FUZZ_META_BITOP_VARIADIC: {
      bool invalid_operation = fuzz_consume_bool(input);
      if (invalid_operation) {
        argv[argc++] = "NOOP";
        *expect_runtime_error = true;
      } else {
        size_t op_index = fuzz_consume_size_in_range(
            input, 0, (sizeof(FUZZ_BITOP_VARIADIC_OPS) / sizeof(FUZZ_BITOP_VARIADIC_OPS[0])) - 1);
        argv[argc++] = FUZZ_BITOP_VARIADIC_OPS[op_index];
      }
      argv[argc++] = "dest";
      argv[argc++] = "src1";
      argv[argc++] = "src2";
      if (fuzz_consume_bool(input)) {
        argv[argc++] = "src3";
      }
      break;
    }
    case FUZZ_META_BITOP_NOT:
      argv[argc++] = fuzz_consume_bool(input) ? "NOT" : "NOOP";
      argv[argc++] = "dest";
      argv[argc++] = "src1";
      if (strcmp(argv[1], "NOT") != 0) {
        *expect_runtime_error = true;
      } else if (fuzz_consume_bool(input)) {
        argv[argc++] = "4";
      }
      break;
  }

  return argc;
}

static size_t fuzz_metadata_expected_keys(const FuzzMetadataSpec* spec, const char** argv, int argc,
                                          const char** expected) {
  switch (spec->kind) {
    case FUZZ_META_SINGLE_KEY_ONE:
    case FUZZ_META_SINGLE_KEY_TWO:
    case FUZZ_META_SINGLE_KEY_THREE:
    case FUZZ_META_SINGLE_KEY_VARIADIC:
    case FUZZ_META_SINGLE_KEY_OPTIONAL:
      expected[0] = argv[1];
      return 1;
    case FUZZ_META_PAIR_KEYS:
    case FUZZ_META_PAIR_KEYS_OPTIONAL:
      expected[0] = argv[1];
      expected[1] = argv[2];
      return 2;
    case FUZZ_META_DEST_AND_SOURCES:
      expected[0] = argv[1];
      expected[1] = argv[2];
      expected[2] = argv[3];
      return 3;
    case FUZZ_META_BITOP_VARIADIC:
      if (strcmp(argv[1], "NOOP") == 0) {
        return 0;
      }
      for (int i = 2; i < argc; i++) {
        expected[i - 2] = argv[i];
      }
      return (size_t)(argc - 2);
    case FUZZ_META_BITOP_NOT:
      if (strcmp(argv[1], "NOT") != 0) {
        return 0;
      }
      expected[0] = argv[2];
      expected[1] = argv[3];
      if (argc > 4) {
        // COMMAND GETKEYS treats the optional trailing "last" argument as a key.
        expected[2] = argv[4];
        return 3;
      }
      return 2;
  }

  return 0;
}

static size_t fuzz_metadata_expected_key_flags(const FuzzMetadataSpec* spec, const char** argv, int argc,
                                               FuzzKeyFlags* expected) {
  switch (spec->kind) {
    case FUZZ_META_SINGLE_KEY_ONE:
    case FUZZ_META_SINGLE_KEY_TWO:
    case FUZZ_META_SINGLE_KEY_THREE:
    case FUZZ_META_SINGLE_KEY_VARIADIC:
    case FUZZ_META_SINGLE_KEY_OPTIONAL:
      expected[0] = spec->primary_flags;
      return 1;
    case FUZZ_META_PAIR_KEYS:
    case FUZZ_META_PAIR_KEYS_OPTIONAL:
      expected[0] = spec->primary_flags;
      expected[1] = spec->secondary_flags;
      return 2;
    case FUZZ_META_DEST_AND_SOURCES:
      expected[0] = spec->primary_flags;
      expected[1] = spec->secondary_flags;
      expected[2] = spec->secondary_flags;
      return 3;
    case FUZZ_META_BITOP_VARIADIC:
      if (strcmp(argv[1], "NOOP") == 0) {
        return 0;
      }
      expected[0] = spec->primary_flags;
      for (int i = 3; i < argc; i++) {
        expected[i - 2] = spec->secondary_flags;
      }
      return (size_t)(argc - 2);
    case FUZZ_META_BITOP_NOT:
      if (strcmp(argv[1], "NOT") != 0) {
        return 0;
      }
      expected[0] = spec->primary_flags;
      expected[1] = spec->secondary_flags;
      return 2;
  }

  return 0;
}

static FuzzKeyFlags fuzz_metadata_reply_flag_mask(redisReply* reply) {
  fuzz_require(reply->type == REDIS_REPLY_ARRAY);

  FuzzKeyFlags flags = 0;
  for (size_t i = 0; i < reply->elements; i++) {
    fuzz_require(reply->element[i]->type == REDIS_REPLY_STRING || reply->element[i]->type == REDIS_REPLY_STATUS);

    const char* flag = reply->element[i]->str;
    if (strcmp(flag, "RO") == 0) {
      flags |= FUZZ_KEYFLAG_RO;
    } else if (strcmp(flag, "RW") == 0) {
      flags |= FUZZ_KEYFLAG_RW;
    } else if (strcmp(flag, "OW") == 0) {
      flags |= FUZZ_KEYFLAG_OW;
    } else if (strcmp(flag, "insert") == 0) {
      flags |= FUZZ_KEYFLAG_INSERT;
    } else if (strcmp(flag, "update") == 0) {
      flags |= FUZZ_KEYFLAG_UPDATE;
    } else if (strcmp(flag, "access") == 0) {
      flags |= FUZZ_KEYFLAG_ACCESS;
    } else if (strcmp(flag, "delete") == 0) {
      flags |= FUZZ_KEYFLAG_DELETE;
    } else {
      fuzz_require(false);
    }
  }

  return flags;
}

static void fuzz_metadata_require_key_reply(redisReply* reply, const char** expected, size_t expected_count) {
  fuzz_reply_require(reply);

  if (expected_count == 0 && reply->type == REDIS_REPLY_ERROR) {
    return;
  }

  fuzz_require(reply->type == REDIS_REPLY_ARRAY);
  fuzz_require(reply->elements == expected_count);
  for (size_t i = 0; i < expected_count; i++) {
    fuzz_require(reply->element[i]->type == REDIS_REPLY_STRING || reply->element[i]->type == REDIS_REPLY_STATUS);
    fuzz_require(strcmp(reply->element[i]->str, expected[i]) == 0);
  }
}

static void fuzz_metadata_require_key_flags_reply(redisReply* reply, const char** expected_keys,
                                                  const FuzzKeyFlags* expected_flags, size_t expected_count) {
  fuzz_reply_require(reply);

  if (expected_count == 0 && reply->type == REDIS_REPLY_ERROR) {
    return;
  }

  fuzz_require(reply->type == REDIS_REPLY_ARRAY);
  fuzz_require(reply->elements == expected_count);
  for (size_t i = 0; i < expected_count; i++) {
    fuzz_require(reply->element[i]->type == REDIS_REPLY_ARRAY);
    fuzz_require(reply->element[i]->elements == 2);
    fuzz_require(reply->element[i]->element[0]->type == REDIS_REPLY_STRING
        || reply->element[i]->element[0]->type == REDIS_REPLY_STATUS);
    fuzz_require(strcmp(reply->element[i]->element[0]->str, expected_keys[i]) == 0);
    fuzz_require(fuzz_metadata_reply_flag_mask(reply->element[i]->element[1]) == expected_flags[i]);
  }
}

static void fuzz_metadata_require_runtime_error(FuzzRedisServer* server, const char** argv, int argc) {
  redisReply* reply = fuzz_redis_command_argv(server, argc, argv);
  fuzz_reply_require(reply);
  fuzz_require(reply->type == REDIS_REPLY_ERROR);
  fuzz_redis_free_reply(reply);
}

static void fuzz_metadata_require_command_info(FuzzRedisServer* server, const char* command) {
  redisReply* reply = fuzz_redis_command(server, "COMMAND INFO %s", command);
  fuzz_reply_require(reply);
  fuzz_require(reply->type == REDIS_REPLY_ARRAY);
  fuzz_require(reply->elements == 1);
  fuzz_require(reply->element[0]->type == REDIS_REPLY_ARRAY);
  fuzz_require(reply->element[0]->elements > 0);
  fuzz_require(reply->element[0]->element[0]->type == REDIS_REPLY_STRING);
  fuzz_require(strcmp(reply->element[0]->element[0]->str, command) == 0);
  fuzz_redis_free_reply(reply);
}

static int fuzz_metadata_invalid_argc(const FuzzMetadataSpec* spec) {
  switch (spec->kind) {
    case FUZZ_META_SINGLE_KEY_ONE:
    case FUZZ_META_SINGLE_KEY_OPTIONAL:
      return 1;
    case FUZZ_META_SINGLE_KEY_TWO:
    case FUZZ_META_SINGLE_KEY_VARIADIC:
    case FUZZ_META_PAIR_KEYS:
    case FUZZ_META_PAIR_KEYS_OPTIONAL:
    case FUZZ_META_DEST_AND_SOURCES:
      return 2;
    case FUZZ_META_SINGLE_KEY_THREE:
      return 3;
    case FUZZ_META_BITOP_VARIADIC:
      return 4;
    case FUZZ_META_BITOP_NOT:
      return 3;
  }

  return 1;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0) {
    return 0;
  }

  FuzzInput input;
  fuzz_input_init(&input, data, size);

  FuzzRedisServer* server = fuzz_metadata_server();
  fuzz_redis_flushall(server);
  fuzz_redis_set_sentinel(server);

  const FuzzMetadataSpec* spec =
      &FUZZ_METADATA_SPECS[fuzz_consume_size_in_range(&input, 0,
          (sizeof(FUZZ_METADATA_SPECS) / sizeof(FUZZ_METADATA_SPECS[0])) - 1)];

  const char* argv[16];
  const char* expected_keys[8];
  FuzzKeyFlags expected_flags[8];
  bool expect_runtime_error = false;

  int argc = fuzz_metadata_build_valid_argv(spec, &input, argv, &expect_runtime_error);
  size_t expected_key_count = fuzz_metadata_expected_keys(spec, argv, argc, expected_keys);
  size_t expected_flag_count = fuzz_metadata_expected_key_flags(spec, argv, argc, expected_flags);
  bool invalid_arity = fuzz_consume_bool(&input);

  if (invalid_arity && argc > 1) {
    argc = fuzz_metadata_invalid_argc(spec);
    expected_key_count = 0;
    expected_flag_count = 0;
    expect_runtime_error = true;
  }

  const char* getkeys_argv[18];
  getkeys_argv[0] = "COMMAND";
  getkeys_argv[1] = "GETKEYS";
  for (int i = 0; i < argc; i++) {
    getkeys_argv[i + 2] = argv[i];
  }

  const char* getkeysandflags_argv[18];
  getkeysandflags_argv[0] = "COMMAND";
  getkeysandflags_argv[1] = "GETKEYSANDFLAGS";
  for (int i = 0; i < argc; i++) {
    getkeysandflags_argv[i + 2] = argv[i];
  }

  long long dbsize_before = fuzz_redis_dbsize(server);
  fuzz_metadata_require_command_info(server, spec->command);
  fuzz_require(fuzz_redis_dbsize(server) == dbsize_before);

  redisReply* reply = fuzz_redis_command_argv(server, argc + 2, getkeys_argv);
  fuzz_metadata_require_key_reply(reply, expected_keys, expected_key_count);
  fuzz_redis_free_reply(reply);
  fuzz_require(fuzz_redis_dbsize(server) == dbsize_before);

  reply = fuzz_redis_command_argv(server, argc + 2, getkeysandflags_argv);
  fuzz_metadata_require_key_flags_reply(reply, expected_keys, expected_flags, expected_flag_count);
  fuzz_redis_free_reply(reply);
  fuzz_require(fuzz_redis_dbsize(server) == dbsize_before);

  if (expect_runtime_error) {
    fuzz_metadata_require_runtime_error(server, argv, argc);
    fuzz_require(fuzz_redis_dbsize(server) == dbsize_before);
  }

  redisReply* ping = fuzz_redis_command(server, "PING");
  fuzz_redis_expect_status(ping, "PONG");
  fuzz_redis_free_reply(ping);

  return 0;
}
