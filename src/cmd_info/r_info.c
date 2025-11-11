#include "redismodule.h"
#include "common.h"

// ===============================
// R.SETBIT key offset value
// ===============================
static const RedisModuleCommandKeySpec R_SETBIT_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_UPDATE,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_SETBIT_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "offset", .type = REDISMODULE_ARG_TYPE_INTEGER},
  {
    .name = "value",
    .type = REDISMODULE_ARG_TYPE_ONEOF,
    .subargs =
      (RedisModuleCommandArg[]){
        {.name = "set", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "1"},
        {.name = "unset", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "0"},
        {0},
      }
  },
  {0} };

static const RedisModuleCommandInfo R_SETBIT_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Sets the specified bit in a roaring key to a value of 1 or 0 and returns the original bit value",
  .complexity = "O(1)",
  .since = "1.0.0",
  .arity = 4,
  .key_specs = (RedisModuleCommandKeySpec*) R_SETBIT_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_SETBIT_ARGS,
};

// ===============================
// R.GETBIT key offset
// ===============================
static const RedisModuleCommandKeySpec R_GETBIT_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_GETBIT_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "offset", .type = REDISMODULE_ARG_TYPE_INTEGER},
  {0} };

static const RedisModuleCommandInfo R_GETBIT_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Retrieves the value of the specified bit from a Roaring key",
  .complexity = "O(1)",
  .since = "1.0.0",
  .arity = 3,
  .key_specs = (RedisModuleCommandKeySpec*) R_GETBIT_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_GETBIT_ARGS,
};

// ===============================
// R.GETBITS key offset [offset...]
// ===============================
static const RedisModuleCommandKeySpec R_GETBITS_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_GETBITS_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "offset", .type = REDISMODULE_ARG_TYPE_INTEGER, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
  {0} };

static const RedisModuleCommandInfo R_GETBITS_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Retrieves multiple values of the specified bit from a Roaring key",
  .complexity = "O(N), where n is the number of offsets",
  .since = "1.0.0",
  .arity = -3,
  .key_specs = (RedisModuleCommandKeySpec*) R_GETBITS_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_GETBITS_ARGS,
};

// ===============================
// R.CLEARBITS key offset [offset...]
// ===============================
static const RedisModuleCommandKeySpec R_CLEARBITS_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_UPDATE,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_CLEARBITS_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "offset", .type = REDISMODULE_ARG_TYPE_INTEGER, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
  {0} };

static const RedisModuleCommandInfo R_CLEARBITS_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Sets the value of the specified bit in a Roaring key to 0",
  .complexity = "O(N), where n is the number of offsets",
  .since = "1.0.0",
  .arity = -3,
  .key_specs = (RedisModuleCommandKeySpec*) R_CLEARBITS_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_CLEARBITS_ARGS,
};

// ===============================
// R.SETINTARRAY key value [value...]
// ===============================
static const RedisModuleCommandKeySpec R_SETINTARRAY_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_INSERT,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_SETINTARRAY_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "value", .type = REDISMODULE_ARG_TYPE_INTEGER, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
  {0} };

static const RedisModuleCommandInfo R_SETINTARRAY_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Creates a Roaring key based on the specified integer array",
  .complexity = "O(N), where n is the number of values",
  .since = "1.0.0",
  .arity = -3,
  .key_specs = (RedisModuleCommandKeySpec*) R_SETINTARRAY_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_SETINTARRAY_ARGS,
};

// ===============================
// R.GETINTARRAY key
// ===============================
static const RedisModuleCommandKeySpec R_GETINTARRAY_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_GETINTARRAY_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {0} };

static const RedisModuleCommandInfo R_GETINTARRAY_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Return an integer array from a roaring bitmap",
  .complexity = "O(N), where n is the number of values",
  .since = "1.0.0",
  .arity = 2,
  .key_specs = (RedisModuleCommandKeySpec*) R_GETINTARRAY_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_GETINTARRAY_ARGS,
};

// ===============================
// R.RANGEINTARRAY key start end
// ===============================
static const RedisModuleCommandKeySpec R_RANGEINTARRAY_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_RANGEINTARRAY_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "start", .type = REDISMODULE_ARG_TYPE_INTEGER},
  {.name = "end", .type = REDISMODULE_ARG_TYPE_INTEGER},
  {0} };

static const RedisModuleCommandInfo R_RANGEINTARRAY_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Returns the offsets of the bits that have a value of 1 within the specified range",
  .complexity = "O(N), where n is the range (end - start)",
  .since = "1.0.0",
  .arity = 4,
  .key_specs = (RedisModuleCommandKeySpec*) R_RANGEINTARRAY_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_RANGEINTARRAY_ARGS,
};

// ===============================
// R.APPENDINTARRAY key value [value...]
// ===============================
static const RedisModuleCommandKeySpec R_APPENDINTARRAY_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_INSERT,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_APPENDINTARRAY_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "value", .type = REDISMODULE_ARG_TYPE_INTEGER, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
  {0} };

static const RedisModuleCommandInfo R_APPENDINTARRAY_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Sets the value of the specified bit in a Roaring key to 1",
  .complexity = "O(N), where n is the number of values",
  .since = "1.0.0",
  .arity = -3,
  .key_specs = (RedisModuleCommandKeySpec*) R_APPENDINTARRAY_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_APPENDINTARRAY_ARGS,
};

// ===============================
// R.DELETEINTARRAY key value [value...]
// ===============================
static const RedisModuleCommandKeySpec R_DELETEINTARRAY_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_DELETE,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_DELETEINTARRAY_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "value", .type = REDISMODULE_ARG_TYPE_INTEGER, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
  {0} };

static const RedisModuleCommandInfo R_DELETEINTARRAY_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Sets the value of the specified bit in a Roaring key to 0",
  .complexity = "O(N), where n is the number of values",
  .since = "1.0.0",
  .arity = -3,
  .key_specs = (RedisModuleCommandKeySpec*) R_DELETEINTARRAY_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_DELETEINTARRAY_ARGS,
};

// ===============================
// R.DIFF destkey key1 key2
// ===============================
static const RedisModuleCommandKeySpec R_DIFF_KEYSPECS[] = {
  {
    .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
    .bs.index.pos = 1,
    .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
    .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0},
    .flags = REDISMODULE_CMD_KEY_OW | REDISMODULE_CMD_KEY_INSERT
  },
  {
    .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
    .bs.index.pos = 2,
    .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
    .fk.range = {.lastkey = 2, .keystep = 1, .limit = 0},
    .flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS
  },
  {0} };

static const RedisModuleCommandArg R_DIFF_ARGS[] = {
  {.name = "destkey", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "key1", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 1},
  {.name = "key2", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 1},
  {0} };

static const RedisModuleCommandInfo R_DIFF_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Computes the difference between two Roaring bitmaps and stores the result in destkey",
  .complexity = "O(N), where n is the number of values",
  .since = "1.0.0",
  .arity = 4,
  .key_specs = (RedisModuleCommandKeySpec*) R_DIFF_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_DIFF_ARGS,
};

// ===============================
// R.SETFULL key
// ===============================
static const RedisModuleCommandKeySpec R_SETFULL_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_OW | REDISMODULE_CMD_KEY_INSERT,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_SETFULL_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {0} };

static const RedisModuleCommandInfo R_SETFULL_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Fill up a roaring bitmap in integer",
  .complexity = "O(UINT32_MAX)",
  .since = "1.0.0",
  .arity = 2,
  .key_specs = (RedisModuleCommandKeySpec*) R_SETFULL_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_SETFULL_ARGS,
};

// ===============================
// R.SETRANGE key start end
// ===============================
static const RedisModuleCommandKeySpec R_SETRANGE_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_UPDATE,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_SETRANGE_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "start", .type = REDISMODULE_ARG_TYPE_INTEGER},
  {.name = "end", .type = REDISMODULE_ARG_TYPE_INTEGER},
  {0} };

static const RedisModuleCommandInfo R_SETRANGE_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Sets the bits within the specified range in a Roaring key to a value of 1",
  .complexity = "O(N), where n is the range (end - start)",
  .since = "1.0.0",
  .arity = 4,
  .key_specs = (RedisModuleCommandKeySpec*) R_SETRANGE_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_SETRANGE_ARGS,
};

// ===============================
// R.OPTIMIZE key
// ===============================
static const RedisModuleCommandKeySpec R_OPTIMIZE_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_UPDATE,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_OPTIMIZE_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "mem", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .flags = REDISMODULE_CMD_ARG_OPTIONAL, .token = "MEM"},
  {0} };

static const RedisModuleCommandInfo R_OPTIMIZE_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Optimizes the storage of a Roaring key",
  .complexity = "O(M)",
  .since = "1.0.0",
  .arity = -2,
  .key_specs = (RedisModuleCommandKeySpec*) R_OPTIMIZE_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_OPTIMIZE_ARGS,
};

// ===============================
// R.SETBITARRAY key value
// ===============================
static const RedisModuleCommandKeySpec R_SETBITARRAY_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_INSERT,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_SETBITARRAY_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "value", .type = REDISMODULE_ARG_TYPE_STRING},
  {0} };

static const RedisModuleCommandInfo R_SETBITARRAY_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Creates a Roaring key based on the specified bit array string",
  .complexity = "O(N), where n is the number of values",
  .since = "1.0.0",
  .arity = 3,
  .key_specs = (RedisModuleCommandKeySpec*) R_SETBITARRAY_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_SETBITARRAY_ARGS,
};

// ===============================
// R.GETBITARRAY key
// ===============================
static const RedisModuleCommandKeySpec R_GETBITARRAY_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_GETBITARRAY_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {0} };

static const RedisModuleCommandInfo R_GETBITARRAY_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Returns a string that consists of bit values of 0 and 1 in a Roaring key",
  .complexity = "O(C)",
  .since = "1.0.0",
  .arity = 2,
  .key_specs = (RedisModuleCommandKeySpec*) R_GETBITARRAY_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_GETBITARRAY_ARGS,
};

// ===============================
// R.BITOP operation destkey key1 key2 [key...]
// ===============================
static const RedisModuleCommandKeySpec R_BITOP_KEYSPECS[] = {
  {
    .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
    .bs.index.pos = 2,
    .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
    .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0},
    .flags = REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_INSERT
  },
  {
    .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
    .bs.index.pos = 3,
    .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
    .fk.range = {.lastkey = -1, .keystep = 1, .limit = 0},
    .flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS
  },
  {0}
};

static const RedisModuleCommandArg R_BITOP_ARGS[] = {
  {.name = "operation", .type = REDISMODULE_ARG_TYPE_STRING},
  {.name = "destkey", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 1, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
  {0}
};

static const RedisModuleCommandInfo R_BITOP_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Performs set operations on Roaring Bitmaps and stores the result in destkey",
  .complexity = "O(N), where N is the number of keys",
  .since = "1.0.0",
  .arity = -4,
  .key_specs = (RedisModuleCommandKeySpec*) R_BITOP_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_BITOP_ARGS,
};

// ===============================
// R.BITCOUNT key
// ===============================
static const RedisModuleCommandKeySpec R_BITCOUNT_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_BITCOUNT_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {0} };

static const RedisModuleCommandInfo R_BITCOUNT_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Counts the number of bits that have a value of 1",
  .complexity = "O(M)",
  .since = "1.0.0",
  .arity = 2,
  .key_specs = (RedisModuleCommandKeySpec*) R_BITCOUNT_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_BITCOUNT_ARGS,
};

// ===============================
// R.BITPOS key value
// ===============================
static const RedisModuleCommandKeySpec R_BITPOS_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_BITPOS_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {
    .name = "value",
    .type = REDISMODULE_ARG_TYPE_ONEOF,
    .subargs =
      (RedisModuleCommandArg[]){
        {.name = "set", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "1"},
        {.name = "unset", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "0"},
        {0},
      }
  },
  {0} };

static const RedisModuleCommandInfo R_BITPOS_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Return the position of the first bit set to 1 or 0",
  .complexity = "O(C)",
  .since = "1.0.0",
  .arity = 3,
  .key_specs = (RedisModuleCommandKeySpec*) R_BITPOS_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_BITPOS_ARGS,
};

// ===============================
// R.MIN key
// ===============================
static const RedisModuleCommandKeySpec R_MIN_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_MIN_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {0} };

static const RedisModuleCommandInfo R_MIN_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Retrieves the offset of the first bit that has a value of 1 in a Roaring key",
  .complexity = "O(1)",
  .since = "1.0.0",
  .arity = 2,
  .key_specs = (RedisModuleCommandKeySpec*) R_MIN_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_MIN_ARGS,
};

// ===============================
// R.MAX key
// ===============================
static const RedisModuleCommandKeySpec R_MAX_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_MAX_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {0} };

static const RedisModuleCommandInfo R_MAX_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Retrieves the offset of the last bit that has a value of 1 in a Roaring key",
  .complexity = "O(1)",
  .since = "1.0.0",
  .arity = 2,
  .key_specs = (RedisModuleCommandKeySpec*) R_MAX_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_MAX_ARGS,
};

// ===============================
// R.CLEAR key
// ===============================
static const RedisModuleCommandKeySpec R_CLEAR_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_OW | REDISMODULE_CMD_KEY_DELETE,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_CLEAR_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {0} };

static const RedisModuleCommandInfo R_CLEAR_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Cleanup Roaring key",
  .complexity = "O(M)",
  .since = "1.0.0",
  .arity = 2,
  .key_specs = (RedisModuleCommandKeySpec*) R_CLEAR_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_CLEAR_ARGS,
};

// ===============================
// R.CONTAINS key1 key2 [ALL|ALL_STRICT|EQ]
// ===============================
static const RedisModuleCommandKeySpec R_CONTAINS_KEYSPECS[] = {
  {
    .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
    .bs.index.pos = 1,
    .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
    .fk.range = {.lastkey = 1, .keystep = 1, .limit = 0},
    .flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS
  },
  {0}
};

static const RedisModuleCommandArg R_CONTAINS_ARGS[] = {
  {.name = "key1", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "key2", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {
    .name = "mode",
    .type = REDISMODULE_ARG_TYPE_ONEOF,
    .flags = REDISMODULE_CMD_ARG_OPTIONAL,
    .subargs =
      (RedisModuleCommandArg[]){
        {.name = "ALL", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "ALL"},
        {.name = "ALL_STRICT", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "ALL_STRICT"},
        {.name = "EQ", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "EQ"},
        {0},
      }
  },
  {0}
};

static const RedisModuleCommandInfo R_CONTAINS_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Check whether two bitmaps intersect",
  .complexity = "O(C)",
  .since = "1.0.0",
  .arity = -3,
  .key_specs = (RedisModuleCommandKeySpec*) R_CONTAINS_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_CONTAINS_ARGS,
};

// ===============================
// R.JACCARD key1 key2
// ===============================
static const RedisModuleCommandKeySpec R_JACCARD_KEYSPECS[] = {
  {
    .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
    .bs.index.pos = 1,
    .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
    .fk.range = {.lastkey = 1, .keystep = 1, .limit = 0},
    .flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS
  },
  {0}
};

static const RedisModuleCommandArg R_JACCARD_ARGS[] = {
  {.name = "key1", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {.name = "key2", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {0}
};

static const RedisModuleCommandInfo R_JACCARD_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Check whether two bitmaps intersect",
  .complexity = "O(C)",
  .since = "1.0.0",
  .arity = 3,
  .key_specs = (RedisModuleCommandKeySpec*) R_JACCARD_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_JACCARD_ARGS,
};

int RegisterRCommandInfos(RedisModuleCtx* ctx) {
  SetCommandInfo(ctx, "R.SETBIT", &R_SETBIT_INFO);
  SetCommandInfo(ctx, "R.GETBIT", &R_GETBIT_INFO);
  SetCommandInfo(ctx, "R.GETBITS", &R_GETBITS_INFO);
  SetCommandInfo(ctx, "R.CLEARBITS", &R_CLEARBITS_INFO);
  SetCommandInfo(ctx, "R.SETINTARRAY", &R_SETINTARRAY_INFO);
  SetCommandInfo(ctx, "R.GETINTARRAY", &R_GETINTARRAY_INFO);
  SetCommandInfo(ctx, "R.RANGEINTARRAY", &R_RANGEINTARRAY_INFO);
  SetCommandInfo(ctx, "R.APPENDINTARRAY", &R_APPENDINTARRAY_INFO);
  SetCommandInfo(ctx, "R.DELETEINTARRAY", &R_DELETEINTARRAY_INFO);
  SetCommandInfo(ctx, "R.DIFF", &R_DIFF_INFO);
  SetCommandInfo(ctx, "R.SETFULL", &R_SETFULL_INFO);
  SetCommandInfo(ctx, "R.SETRANGE", &R_SETRANGE_INFO);
  SetCommandInfo(ctx, "R.OPTIMIZE", &R_OPTIMIZE_INFO);
  SetCommandInfo(ctx, "R.SETBITARRAY", &R_SETBITARRAY_INFO);
  SetCommandInfo(ctx, "R.GETBITARRAY", &R_GETBITARRAY_INFO);
  SetCommandInfo(ctx, "R.BITOP", &R_BITOP_INFO);
  SetCommandInfo(ctx, "R.BITCOUNT", &R_BITCOUNT_INFO);
  SetCommandInfo(ctx, "R.BITPOS", &R_BITPOS_INFO);
  SetCommandInfo(ctx, "R.MIN", &R_MIN_INFO);
  SetCommandInfo(ctx, "R.MAX", &R_MAX_INFO);
  SetCommandInfo(ctx, "R.CLEAR", &R_CLEAR_INFO);
  SetCommandInfo(ctx, "R.CONTAINS", &R_CONTAINS_INFO);
  SetCommandInfo(ctx, "R.JACCARD", &R_JACCARD_INFO);

  return REDISMODULE_OK;
}
