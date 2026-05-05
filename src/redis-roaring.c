#include "redismodule.h"
#include "r_32.h"
#include "r_64.h"
#include "rmalloc.h"
#include "common.h"
#include "cmd_info/command_info.h"
#include "parse.h"
#include "version.h"

int RStatBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  int output_format = BITMAP_STATISTICS_FORMAT_PLAIN_TEXT;

  if (argc == 3) {
    const char* format_arg = RedisModule_StringPtrLen(argv[2], NULL);

    if (strcmp(format_arg, "JSON") == 0) {
      output_format = BITMAP_STATISTICS_FORMAT_JSON;
    }
  } else if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_ReplyWithNull(ctx);
    return REDISMODULE_OK;
  }

  RedisModuleType* type = RedisModule_ModuleTypeGetType(key);

  char* stat = NULL;
  int stat_len;

  if (type == BitmapType) {
    Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
    stat = bitmap_statistics_str(bitmap, output_format, &stat_len);
  } else if (type == Bitmap64Type) {
    Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
    stat = bitmap64_statistics_str(bitmap, output_format, &stat_len);
  } else {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  if (stat == NULL) {
    RedisModule_Log(ctx, "warning", "Failed to allocate R.START resulting string");
    return REDISMODULE_ERR;
  }

  RedisModule_ReplyWithVerbatimString(ctx, stat, stat_len);
  free(stat); // use system free() instead of rm_free() because asprintf uses system malloc

  return REDISMODULE_OK;
}

void RedisModule_OnShutdown(RedisModuleCtx* ctx, RedisModuleEvent e, uint64_t sub, void* data) {
  REDISMODULE_NOT_USED(e);
  REDISMODULE_NOT_USED(data);
  REDISMODULE_NOT_USED(sub);

  R32Module_onShutdown(ctx, e, sub, data);
  R64Module_onShutdown(ctx, e, sub, data);
}

int RedisModule_OnLoad(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  // Register the module itself
  if (RedisModule_Init(ctx, REDISROARING_MODULE_NAME, REDISROARING_MODULE_VERSION, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  RedisModule_Log(ctx,
    "notice",
    "RedisRoaring version %d",
    REDISROARING_MODULE_VERSION);

  RedisModule_SubscribeToServerEvent(ctx, RedisModuleEvent_Shutdown, RedisModule_OnShutdown);

  // Setup roaring with Redis trackable memory allocation wrapper
  roaring_memory_t roaring_memory_hook = {
    .malloc = rm_malloc,
    .realloc = rm_realloc,
    .calloc = rm_calloc,
    .free = rm_free,
    .aligned_malloc = rm_aligned_malloc,
    .aligned_free = rm_free,
  };

  roaring_init_memory_hook(roaring_memory_hook);

  if (R32Module_onLoad(ctx, argv, argc) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (R64Module_onLoad(ctx, argv, argc) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  // Register general commands
#define RegisterCommand(ctx, name, cmd, mode, acl)                                                 \
  RegisterCommandWithModesAndAcls(ctx, name, cmd, mode, acl " roaring");

  RegisterCommand(ctx, "R.STAT", RStatBitCommand, "readonly", "read");

  if (RegisterRootCommandInfos(ctx) != REDISMODULE_OK) {
    RedisModule_Log(ctx, "warning", "Failed to register the general commands info");
    return REDISMODULE_ERR;
  }
#undef RegisterCommand

  return REDISMODULE_OK;
}
