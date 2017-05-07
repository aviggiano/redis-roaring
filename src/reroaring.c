#include "redismodule.h"
#include "roaring.h"

#define BITMAP_ENCODING_VERSION 1

static RedisModuleType* BitmapType;

/* === Internal data structure === */
typedef roaring_bitmap_t Bitmap;

Bitmap* bitmap_alloc() {
  return roaring_bitmap_create();
}

void bitmap_free(Bitmap* bitmap) {
  roaring_bitmap_free(bitmap);
}

void bitmap_setbit(Bitmap* bitmap, uint32_t offset, char value) {
  if (value == 0) {
    roaring_bitmap_remove(bitmap, offset);
  }
  else {
    roaring_bitmap_add(bitmap, offset);
  }
}

char bitmap_getbit(Bitmap* bitmap, uint32_t offset) {
  return roaring_bitmap_contains(bitmap, offset) == true;
}

/* === Bitmap type methods === */

void BitmapRdbSave(RedisModuleIO* rdb, void* value) {
  Bitmap* bitmap = value;
  size_t serialized_size = roaring_bitmap_size_in_bytes(bitmap);
  char* serialized_bitmap = RedisModule_Alloc(serialized_size);
  size_t serialized_size_check = roaring_bitmap_serialize(bitmap, serialized_bitmap);

  RedisModule_SaveStringBuffer(rdb, serialized_bitmap, serialized_size_check);

  RedisModule_Free(serialized_bitmap);
}

void* BitmapRdbLoad(RedisModuleIO* rdb, int encver) {
  if (encver != BITMAP_ENCODING_VERSION) {
    RedisModule_LogIOError(rdb, "warning", "Can't load data with version %d", encver);
    return NULL;
  }
  size_t size;
  char* serialized_bitmap = RedisModule_LoadStringBuffer(rdb, &size);
  Bitmap* bitmap = roaring_bitmap_deserialize(serialized_bitmap);
  return bitmap;
}

typedef struct Bitmap_aof_rewrite_callback_params_s {
  RedisModuleIO* aof;
  RedisModuleString* key;
} Bitmap_aof_rewrite_callback_params;

bool BitmapAofRewriteCallback(uint32_t offset, void* param) {
  Bitmap_aof_rewrite_callback_params* params = param;
  RedisModule_EmitAOF(params->aof, "R.SETBIT", "sll", params->key, offset, 1);
  return true;
}

void BitmapAofRewrite(RedisModuleIO* aof, RedisModuleString* key, void* value) {
  Bitmap* bitmap = value;
  Bitmap_aof_rewrite_callback_params params = {
      aof: aof,
      key: key
  };
  roaring_iterate(bitmap, BitmapAofRewriteCallback, &params);
}

size_t BitmapMemUsage(const void* value) {
  const Bitmap* bitmap = value;
  return roaring_bitmap_size_in_bytes(bitmap);
}

void BitmapFree(void* value) {
  bitmap_free(value);
}

/* === Bitmap type commands === */

Bitmap* BitmapUpget(RedisModuleKey* key, int type) {
  /* Create an empty value object if the key is currently empty. */
  Bitmap* bitmap;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    bitmap = bitmap_alloc();
    RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
  } else {
    bitmap = RedisModule_ModuleTypeGetValue(key);
  }
  return bitmap;
}

/**
 * R.SETBIT <key> <offset> <value>
 * */
int RSetBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  long long offset;
  if ((RedisModule_StringToLongLong(argv[2], &offset) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid offset: must be an unsigned 32 bit integer");
  }
  long long value;
  if ((RedisModule_StringToLongLong(argv[2], &value) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid offset: must be either 0 or 1");
  }

  Bitmap* bitmap = BitmapUpget(key, type);

  /* Set bit with value */
  bitmap_setbit(bitmap, (uint32_t) offset, (char) value);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}

/**
 * R.GETBIT <key> <offset>
 * */
int RGetBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  long long offset;
  if ((RedisModule_StringToLongLong(argv[2], &offset) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid offset: must be an unsigned 32 bit integer");
  }

  Bitmap* bitmap = BitmapUpget(key, type);

  /* Get bit */
  char value = bitmap_getbit(bitmap, (uint32_t) offset);

  RedisModule_ReplyWithLongLong(ctx, value);

  return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx* ctx) {
  // Register the module itself
  if (RedisModule_Init(ctx, "REROARING", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  RedisModuleTypeMethods tm = {
      .version = REDISMODULE_TYPE_METHOD_VERSION,
      .rdb_load = BitmapRdbLoad,
      .rdb_save = BitmapRdbSave,
      .aof_rewrite = BitmapAofRewrite,
      .mem_usage = BitmapMemUsage,
      .free = BitmapFree
  };

  BitmapType = RedisModule_CreateDataType(ctx, "reroaring", 0, &tm);
  if (BitmapType == NULL) return REDISMODULE_ERR;

  // register our commands
  if (RedisModule_CreateCommand(ctx, "R.SETBIT", RSetBitCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.GETBIT", RGetBitCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
