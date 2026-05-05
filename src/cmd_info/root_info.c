#include <string.h>

#include "redismodule.h"
#include "common.h"

// ===============================
// R.STAT key [TEXT|JSON]
// ===============================
static const RedisModuleCommandKeySpec R_STAT_KEYSPECS[] = {
  {.flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS,
   .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
   .bs.index = {.pos = 1},
   .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
   .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
  {0} };

static const RedisModuleCommandArg R_STAT_ARGS[] = {
  {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
  {
    .name = "format",
    .type = REDISMODULE_ARG_TYPE_ONEOF,
    .flags = REDISMODULE_CMD_ARG_OPTIONAL,
    .subargs =
      (RedisModuleCommandArg[]){
        {.name = "TEXT", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "TEXT"},
        {.name = "JSON", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "JSON"},
        {0},
      }
  },
  {0} };

static const RedisModuleCommandInfo R_STAT_INFO = {
  .version = REDISMODULE_COMMAND_INFO_VERSION,
  .summary = "Returns statistical information about a Roaring bitmap, including container counts, memory usage, and cardinality metrics",
  .complexity = "O(1)",
  .since = "1.0.0",
  .arity = -2,
  .key_specs = (RedisModuleCommandKeySpec*) R_STAT_KEYSPECS,
  .args = (RedisModuleCommandArg*) R_STAT_ARGS,
};

int RegisterRootCommandInfos(RedisModuleCtx* ctx) {
  SetCommandInfo(ctx, "R.STAT", &R_STAT_INFO);
  return REDISMODULE_OK;
}

const RedisModuleCommandInfo* GetRootCommandInfo(const char* name) {
  if (name != NULL && strcmp(name, "R.STAT") == 0) {
    return &R_STAT_INFO;
  }

  return NULL;
}
