#include "redismodule.h"
#include "data_structure.h"
#include "type.h"

static RedisModuleType* BitmapType;

/* === Bitmap type commands === */

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

  /* Create an empty value object if the key is currently empty. */
  Bitmap* bitmap;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    bitmap = bitmap_alloc();
    RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
  } else {
    bitmap = RedisModule_ModuleTypeGetValue(key);
  }

  /* Set bit with value */
  char old_value = bitmap_getbit(bitmap, (uint32_t) offset);
  bitmap_setbit(bitmap, (uint32_t) offset, (char) value);

  RedisModule_ReplicateVerbatim(ctx);
  // Integer reply: the original bit value stored at offset.
  RedisModule_ReplyWithLongLong(ctx, old_value);

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

  char value;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    value = 0;
  }
  else {
    Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
    /* Get bit */
    value = bitmap_getbit(bitmap, (uint32_t) offset);
  }

  RedisModule_ReplyWithLongLong(ctx, value);

  return REDISMODULE_OK;
}

int RBitOp(RedisModuleCtx* ctx, RedisModuleString** argv, int argc, Bitmap* (* operation)(uint32_t, const Bitmap**)) {
  // open destkey for writing
  RedisModuleKey* destkey = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ | REDISMODULE_WRITE);
  int desttype = RedisModule_KeyType(destkey);
  if (desttype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(destkey) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  uint32_t n = (uint32_t) (argc - 3);
  // checks for srckey types
  for (uint32_t i = 0; i < n; i++) {
    RedisModuleKey* srckey = RedisModule_OpenKey(ctx, argv[3 + i], REDISMODULE_READ);
    int srctype = RedisModule_KeyType(srckey);
    if (srctype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(srckey) != BitmapType) {
      return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }
  }

  // alloc memory for srckey bitmaps
  Bitmap** bitmaps = RedisModule_Alloc(n * sizeof(*bitmaps));
  bool* should_free = RedisModule_Alloc(n * sizeof(*should_free));

  for (uint32_t i = 0; i < n; i++) {
    RedisModuleKey* srckey = RedisModule_OpenKey(ctx, argv[3 + i], REDISMODULE_READ);
    int srctype = RedisModule_KeyType(srckey);

    Bitmap* bitmap;
    if (srctype == REDISMODULE_KEYTYPE_EMPTY) {
      // "non-existent keys [...] are considered as a stream of zero bytes up to the length of the longest string"
      bitmap = bitmap_alloc();
      should_free[i] = true;
    }
    else {
      bitmap = RedisModule_ModuleTypeGetValue(srckey);
      should_free[i] = false;
    }
    bitmaps[i] = bitmap;
  }

  // calculate destkey bitmap
  Bitmap* result;
  if (desttype != REDISMODULE_KEYTYPE_EMPTY) {
    result = RedisModule_ModuleTypeGetValue(destkey);
    // "The result of the operation is always stored at destkey", so we need to free old keys
    bitmap_free(result);
  }
  result = operation(n, (const Bitmap**) bitmaps);
  RedisModule_ModuleTypeSetValue(destkey, BitmapType, result);

  for (uint32_t i = 0; i < n; i++) {
    if (should_free[i]) {
      bitmap_free(bitmaps[i]);
    }
  }
  RedisModule_Free(should_free);
  RedisModule_Free(bitmaps);

  RedisModule_ReplicateVerbatim(ctx);

  // Integer reply: The size of the string stored in the destination key
  // (adapted to cardinality)
  uint64_t cardinality = roaring_bitmap_get_cardinality(result);
  RedisModule_ReplyWithLongLong(ctx, (long long) cardinality);

  return REDISMODULE_OK;
}

/**
 * BITOP AND destkey srckey1 srckey2 srckey3 ... srckeyN
 * BITOP OR destkey srckey1 srckey2 srckey3 ... srckeyN
 * BITOP XOR destkey srckey1 srckey2 srckey3 ... srckeyN
 * BITOP NOT destkey srckey
 * R.BITOP <operation> <destkey> <key> [<key> ...]
 * */
int RBitOpCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 4) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  size_t len;
  const char* operation = RedisModule_StringPtrLen(argv[1], &len);

  if (strcmp(operation, "NOT") == 0) {
    if (argc != 4) return RedisModule_WrongArity(ctx);
    else return RBitOp(ctx, argv, argc, bitmap_not_array);
  }
  else if (strcmp(operation, "AND") == 0) {
    if (argc == 4) return RedisModule_WrongArity(ctx);
    else return RBitOp(ctx, argv, argc, bitmap_and);
  }
  else if (strcmp(operation, "OR") == 0) {
    if (argc == 4) return RedisModule_WrongArity(ctx);
    else return RBitOp(ctx, argv, argc, bitmap_or);
  }
  else if (strcmp(operation, "XOR") == 0) {
    if (argc == 4) return RedisModule_WrongArity(ctx);
    else return RBitOp(ctx, argv, argc, bitmap_xor);
  }
  else {
    RedisModule_ReplyWithSimpleString(ctx, "NOK - TODO: check response");
    return REDISMODULE_ERR;
  }

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
  if (RedisModule_CreateCommand(ctx, "R.BITOP", RBitOpCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
