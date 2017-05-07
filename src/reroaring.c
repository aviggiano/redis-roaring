#include "redismodule.h"
#include "roaring.h"

#define BITMAP_ENCODING_VERSION 0

static RedisModuleType *BitmapType;

/* === Internal data structure === */
typedef struct bitmap {
  uint32_t size;
  char *array;
} Bitmap;

Bitmap *bitmap_alloc() {
  Bitmap *bitmap = RedisModule_Alloc(sizeof(*bitmap));
  bitmap->size = 0;
  bitmap->array = NULL;
  return bitmap;
}

void bitmap_free(Bitmap *bitmap) {
  RedisModule_Free(bitmap->array);
  RedisModule_Free(bitmap);
}

void bitmap_setbit(Bitmap *bitmap, uint32_t offset, char value) {
  if (offset >= bitmap->size) {
    bitmap->size = offset * 2 + 1;
    bitmap->array = RedisModule_Realloc(bitmap->array, bitmap->size);
  }
  bitmap->array[offset] = value != 0;
}

char bitmap_getbit(Bitmap *bitmap, uint32_t offset) {
  if (offset >= bitmap->size) {
    return 0;
  }
  else {
    return bitmap->array[offset];
  }
}

/* === Bitmap type methods === */

void BitmapRdbSave(RedisModuleIO *rdb, void *value) {
  Bitmap *bitmap = value;
  RedisModule_SaveUnsigned(rdb, bitmap->size);
  RedisModule_SaveStringBuffer(rdb, bitmap->array, bitmap->size);
}

void *BitmapRdbLoad(RedisModuleIO *rdb, int encver) {
  if (encver != BITMAP_ENCODING_VERSION) {
    RedisModule_LogIOError(rdb, "warning", "Can't load data with version %d", encver);
    return NULL;
  }
  Bitmap *bitmap = bitmap_alloc();
  bitmap->size = (uint32_t) RedisModule_LoadUnsigned(rdb);
  size_t size;
  bitmap->array = RedisModule_LoadStringBuffer(rdb, &size);
  return bitmap;
}

void BitmapAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
  Bitmap *bitmap = value;
  for (uint32_t i = 0; i < bitmap->size; i++) {
    RedisModule_EmitAOF(aof, "R.SETBIT", "sll", key, i, bitmap->array[i]);
  }
}

size_t BitmapMemUsage(const void *value) {
  const Bitmap *bitmap = value;
  return sizeof(*bitmap) + bitmap->size * sizeof(*bitmap->array);
}

void BitmapFree(void *value) {
  bitmap_free(value);
}

/* === Bitmap type commands === */

Bitmap *BitmapUpget(RedisModuleKey *key, int type) {
  /* Create an empty value object if the key is currently empty. */
  Bitmap *bitmap;
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
int RSetBitCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  roaring_bitmap_t *r1 = roaring_bitmap_create();
  for (uint32_t i = 100; i < 1000; i++) roaring_bitmap_add(r1, i);
  printf("cardinality = %d\n", (int) roaring_bitmap_get_cardinality(r1));
  roaring_bitmap_free(r1);



  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
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

  Bitmap *bitmap = BitmapUpget(key, type);

  /* Set bit with value */
  bitmap_setbit(bitmap, (uint32_t) offset, (char) value);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}

/**
 * R.GETBIT <key> <offset>
 * */
int RGetBitCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  long long offset;
  if ((RedisModule_StringToLongLong(argv[2], &offset) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid offset: must be an unsigned 32 bit integer");
  }

  Bitmap *bitmap = BitmapUpget(key, type);

  /* Get bit */
  char value = bitmap_getbit(bitmap, (uint32_t) offset);

  RedisModule_ReplyWithLongLong(ctx, value);

  return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {
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
