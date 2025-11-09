#pragma once

#include "redismodule.h"
#include "stdbool.h"

#define ERRORMSG_WRONGARG(arg_name, description) "ERR invalid " arg_name ": " description
#define ERRORMSG_WRONGARG_UINT32(arg_name) ERRORMSG_WRONGARG(arg_name, "must be an unsigned 32 bit integer")
#define ERRORMSG_WRONGARG_UINT64(arg_name) ERRORMSG_WRONGARG(arg_name, "must be an unsigned 64 bit integer")
#define ERRORMSG_WRONGARG_BIT(arg_name) ERRORMSG_WRONGARG(arg_name, "must be either 0 or 1")

#define ParseUint32OrReturn(ctx, argv_item, name, out_var) \
  if (!StrToUInt32((argv_item), &(out_var))) { \
    return RedisModule_ReplyWithError((ctx), ERRORMSG_WRONGARG_UINT32(name)); \
  } \

#define ParseUint64OrReturn(ctx, argv_item, name, out_var) \
  if (!StrToUInt64((argv_item), &(out_var))) { \
    return RedisModule_ReplyWithError((ctx), ERRORMSG_WRONGARG_UINT64(name)); \
  } \

#define ParseBoolOrReturn(ctx, argv_item, name, out_var) \
  if (!StrToBool((argv_item), &(out_var))) { \
    return RedisModule_ReplyWithError((ctx), ERRORMSG_WRONGARG_BIT(name)); \
  } \


static char REPLY_UINT64_BUFFER[21];

void ReplyWithUint64(RedisModuleCtx* ctx, uint64_t value);
bool StrToUInt32(const RedisModuleString* str, uint32_t* ull);
bool StrToUInt64(const RedisModuleString* str, uint64_t* ull);
bool StrToBool(const RedisModuleString* str, bool* ull);
