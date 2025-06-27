#include "redismodule.h"
#include "data-structure.h"
#include "sds.h"

#define malloc(size) RedisModule_Alloc(size)
#define calloc(count, size) RedisModule_Calloc(count, size)
#define realloc(p, sz) RedisModule_Realloc(p, sz)
#define free(p) RedisModule_Free(p)

#include "roaring.h"

static RedisModuleType* BitmapType;

#define BITMAP_ENCODING_VERSION 1

void BitmapRdbSave(RedisModuleIO* rdb, void* value);
void* BitmapRdbLoad(RedisModuleIO* rdb, int encver);
void BitmapAofRewrite(RedisModuleIO* aof, RedisModuleString* key, void* value);
size_t BitmapMemUsage(const void* value);
void BitmapFree(void* value);

/* === Bitmap type methods === */
void BitmapRdbSave(RedisModuleIO* rdb, void* value) {
  Bitmap* bitmap = value;
  size_t serialized_max_size = roaring_bitmap_size_in_bytes(bitmap);
  char* serialized_bitmap = RedisModule_Alloc(serialized_max_size);
  size_t serialized_size = roaring_bitmap_serialize(bitmap, serialized_bitmap);
  RedisModule_SaveStringBuffer(rdb, serialized_bitmap, serialized_size);
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
  RedisModule_Free(serialized_bitmap);
  return bitmap;
}

typedef struct Bitmap_aof_rewrite_callback_params_s {
  RedisModuleIO* aof;
  RedisModuleString* key;
} Bitmap_aof_rewrite_callback_params;

static bool BitmapAofRewriteCallback(uint32_t offset, void* param) {
  Bitmap_aof_rewrite_callback_params* params = param;
  RedisModule_EmitAOF(params->aof, "R.SETBIT", "sll", params->key, offset, 1);
  return true;
}

void BitmapAofRewrite(RedisModuleIO* aof, RedisModuleString* key, void* value) {
  Bitmap* bitmap = value;
  Bitmap_aof_rewrite_callback_params params = {
      .aof = aof,
      .key = key
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

/**
 * R.SETFULL <key>
 * */
int RSetFullCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithError(ctx, "key exists");
  }

  Bitmap* bitmap = bitmap_from_range(0, UINT32_MAX);
  //NOTE bitmap from range is an right open interval, to set full bit for key, we need do it manually
  bitmap_setbit(bitmap, UINT32_MAX, 1);
  RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");
  return REDISMODULE_OK;
}

/**
 * R.SETRANGE <key> <start_num> <end_num>
 * */
int RSetRangeCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  long long start_num;
  if ((RedisModule_StringToLongLong(argv[2], &start_num) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid start_num: must be an unsigned 32 bit integer");
  }
  long long end_num;
  if ((RedisModule_StringToLongLong(argv[3], &end_num) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid end_num: must be an unsigned 32 bit integer");
  }
  if (start_num < 0 || end_num < 0 || end_num < start_num) {
    return RedisModule_ReplyWithError(ctx, "start_num and end_num must >=0 and end_num must >= start_num");
  }

  if ((uint32_t) start_num > UINT32_MAX || (uint32_t) end_num > UINT32_MAX) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid end_num: must be an unsigned 32 bit integer");
  }
  Bitmap* bitmap = (type == REDISMODULE_KEYTYPE_EMPTY) ? NULL : RedisModule_ModuleTypeGetValue(key);

  Bitmap* nbitmap = bitmap_from_range((uint32_t) start_num, (uint32_t) end_num);

  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_ModuleTypeSetValue(key, BitmapType, nbitmap);
  } else {
    Bitmap** bitmaps = RedisModule_Alloc(2 * sizeof(*bitmaps));
    bitmaps[0] = bitmap;
    bitmaps[1] = nbitmap;
    Bitmap* result = bitmap_or(2, (const Bitmap**) bitmaps);
    RedisModule_ModuleTypeSetValue(key, BitmapType, result);
    bitmap_free(bitmaps[1]);
    RedisModule_Free(bitmaps);
  }

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");
  return REDISMODULE_OK;
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
  if ((RedisModule_StringToLongLong(argv[3], &value) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be either 0 or 1");
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
  } else {
    Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
    /* Get bit */
    value = bitmap_getbit(bitmap, (uint32_t) offset);
  }

  RedisModule_ReplyWithLongLong(ctx, value);

  return REDISMODULE_OK;
}

/**
 * R.STAT <key>
 * */
int RStatBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithError(ctx, "ERR key does not exist");
  }

  Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
  Bitmap_statistics stat;
  bitmap_statistics(bitmap, &stat);

  sds s = sdsempty();
  s = sdscatprintf(s, "cardinality: %lld\n", (long long) stat.cardinality);
  s = sdscatprintf(s, "number of containers: %d\n", stat.n_containers);
  s = sdscatprintf(s, "max value: %u\n", stat.max_value);
  s = sdscatprintf(s, "min value: %u\n", stat.min_value);

  s = sdscatprintf(s, "number of array containers: %d\n", stat.n_array_containers);
  s = sdscatprintf(s, "\tarray container values: %d\n", stat.n_values_array_containers);
  s = sdscatprintf(s, "\tarray container bytes: %d\n", stat.n_bytes_array_containers);

  s = sdscatprintf(s, "bitset  containers: %d\n", stat.n_bitset_containers);
  s = sdscatprintf(s, "\tbitset  container values: %d\n", stat.n_values_bitset_containers);
  s = sdscatprintf(s, "\tbitset  container bytes: %d\n", stat.n_bytes_bitset_containers);

  s = sdscatprintf(s, "run containers: %d\n", stat.n_run_containers);
  s = sdscatprintf(s, "\trun container values: %d\n", stat.n_values_run_containers);
  s = sdscatprintf(s, "\trun container bytes: %d\n", stat.n_bytes_run_containers);

  RedisModule_ReplyWithVerbatimString(ctx, s, sdslen(s));
  sdsfree(s);

  return REDISMODULE_OK;
}

/**
 * R.OPTIMIZE <key>
 * */
int ROptimizeBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
      return RedisModule_ReplyWithError(ctx, "ERR no such key");
  }
  if (RedisModule_ModuleTypeGetType(key) != BitmapType) {
      return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
  bitmap_optimize(bitmap);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}


/**
 * R.SETINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int RSetIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  size_t length = (size_t)(argc - 2);
  uint32_t* values = RedisModule_Alloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    long long value;
    if ((RedisModule_StringToLongLong(argv[2 + i], &value) != REDISMODULE_OK)) {
      RedisModule_Free(values);
      return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be an unsigned 32 bit integer");
    }
    values[i] = (uint32_t) value;
  }

  Bitmap* bitmap = bitmap_from_int_array(length, values);
  RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);

  RedisModule_Free(values);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");


  return REDISMODULE_OK;
}

/**
 * R.DIFF <dest> <decreasing> <deductible>
 * */
int RDiffCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
   if (argc != 4) {
     return RedisModule_WrongArity(ctx);
   }
   RedisModule_AutoMemory(ctx);

   // open destkey for writing
   RedisModuleKey* destkey = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
   int desttype = RedisModule_KeyType(destkey);
   if (desttype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(destkey) != BitmapType) {
     return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
   }

   // checks for keys types
   for (uint32_t i = 2; i < 4; i++) {
     RedisModuleKey* srckey = RedisModule_OpenKey(ctx, argv[i], REDISMODULE_READ);
     int srctype = RedisModule_KeyType(srckey);
     if (srctype == REDISMODULE_KEYTYPE_EMPTY || RedisModule_ModuleTypeGetType(srckey) != BitmapType) {
       return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
     }
   }

   RedisModuleKey* decreasing = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ);
   Bitmap* decreasing_bitmap = RedisModule_ModuleTypeGetValue(decreasing);
   RedisModuleKey* deductible = RedisModule_OpenKey(ctx, argv[3], REDISMODULE_READ);
   Bitmap* deductible_bitmap = RedisModule_ModuleTypeGetValue(deductible);

   Bitmap* result = roaring_bitmap_andnot(decreasing_bitmap, deductible_bitmap);
   RedisModule_ModuleTypeSetValue(destkey, BitmapType, result);
   RedisModule_ReplyWithSimpleString(ctx, "OK");
   return REDISMODULE_OK;
}


/**
 * R.APPENDINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int RAppendIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap* bitmap = (type == REDISMODULE_KEYTYPE_EMPTY) ? NULL : RedisModule_ModuleTypeGetValue(key);

  size_t length = (size_t)(argc - 2);
  uint32_t* values = RedisModule_Alloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    long long value;
    if ((RedisModule_StringToLongLong(argv[2 + i], &value) != REDISMODULE_OK)) {
      RedisModule_Free(values);
      return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be an unsigned 32 bit integer");
    }
    values[i] = (uint32_t) value;
  }

  Bitmap* nbitmap = bitmap_from_int_array(length, values);

  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_ModuleTypeSetValue(key, BitmapType, nbitmap);
  } else {
    Bitmap** bitmaps = RedisModule_Alloc(2 * sizeof(*bitmaps));
    bitmaps[0] = bitmap;
    bitmaps[1] = nbitmap;
    Bitmap* result = bitmap_or(2, (const Bitmap**) bitmaps);
    RedisModule_ModuleTypeSetValue(key, BitmapType, result);
    bitmap_free(bitmaps[1]);
    RedisModule_Free(bitmaps);
  }

  RedisModule_Free(values);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}

/**
 * R.RANGEINTARRAY <key> <start> <end>
 * */
int RRangeIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  long long start;
  if ((RedisModule_StringToLongLong(argv[2], &start) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid start: must be an unsigned 32 bit integer");
  }

  long long end;
  if ((RedisModule_StringToLongLong(argv[3], &end) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid start: must be an unsigned 32 bit integer");
  }

  if (start < 0 || end < 0) {
    return RedisModule_ReplyWithError(ctx, "ERR start and end must be >= 0");
  }

  Bitmap* bitmap = (type == REDISMODULE_KEYTYPE_EMPTY) ? NULL : RedisModule_ModuleTypeGetValue(key);
  uint32_t* array = NULL;
  size_t n = 0;
  if (bitmap != NULL) {
    long long count = (long long) bitmap_get_cardinality(bitmap);
    if (start > count - 1 || start > end) {
      n = 0;
    } else {
      n = end - start + 1;
    }
    if (n > 0) {
      array = bitmap_range_int_array(bitmap, (size_t) start, n);
    }
  } else {
    n = 0;
  }
  RedisModule_ReplyWithArray(ctx, n);
  for (size_t i = 0; i < n; i++) {
    RedisModule_ReplyWithLongLong(ctx, array[i]);
  }
  if (array != NULL) {
    bitmap_free_int_array(array);
  }
  return REDISMODULE_OK;
}

/**
 * R.GETINTARRAY <key>
 * */
int RGetIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap* bitmap = (type == REDISMODULE_KEYTYPE_EMPTY) ? NULL : RedisModule_ModuleTypeGetValue(key);

  size_t n = 0;
  uint32_t* array = bitmap == NULL ? NULL : bitmap_get_int_array(bitmap, &n);

  RedisModule_ReplyWithArray(ctx, n);

  for (size_t i = 0; i < n; i++) {
    RedisModule_ReplyWithLongLong(ctx, array[i]);
  }

  bitmap_free_int_array(array);

  return REDISMODULE_OK;
}


/**
 * R.SETBITARRAY <key> <value1>
 * */
int RSetBitArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  size_t len;
  const char* array = RedisModule_StringPtrLen(argv[2], &len);
  Bitmap* bitmap = bitmap_from_bit_array(len, array);
  RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}

/**
 * R.GETBITARRAY <key>
 * */
int RGetBitArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  if (type != REDISMODULE_KEYTYPE_EMPTY) {
    Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
    size_t size;
    char* array = bitmap_get_bit_array(bitmap, &size);
    RedisModule_ReplyWithStringBuffer(ctx, array, size);

    bitmap_free_bit_array(array);
  } else {
    RedisModule_ReplyWithSimpleString(ctx, "");
  }

  return REDISMODULE_OK;
}


int RBitOp(RedisModuleCtx* ctx, RedisModuleString** argv, int argc, Bitmap* (* operation)(uint32_t, const Bitmap**)) {
  if (RedisModule_IsKeysPositionRequest(ctx) > 0) {
    if (argc > 4) {
      for (int i = 2; i < argc; i++) {
        RedisModule_KeyAtPos(ctx, i);
      }
    }
    return REDISMODULE_OK;
  }
  if (argc == 4) return RedisModule_WrongArity(ctx);

  // open destkey for writing
  RedisModuleKey* destkey = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ | REDISMODULE_WRITE);
  int desttype = RedisModule_KeyType(destkey);
  if (desttype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(destkey) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  uint32_t n = (uint32_t)(argc - 3);
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
    } else {
      bitmap = RedisModule_ModuleTypeGetValue(srckey);
      should_free[i] = false;
    }
    bitmaps[i] = bitmap;
  }

  // calculate destkey bitmap
  Bitmap* result = operation(n, (const Bitmap**) bitmaps);
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
  uint64_t cardinality = bitmap_get_cardinality(result);
  RedisModule_ReplyWithLongLong(ctx, (long long) cardinality);

  return REDISMODULE_OK;
}


int RBitFlip(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (RedisModule_IsKeysPositionRequest(ctx) > 0) {
    if (argc <= 5) {
      RedisModule_KeyAtPos(ctx, 2);
      RedisModule_KeyAtPos(ctx, 3);
    }
    return REDISMODULE_OK;
  }

  long long last = -1;
  if (argc == 5) {
    if ((RedisModule_StringToLongLong(argv[4], &last) != REDISMODULE_OK) || last < 0) {
      return RedisModule_ReplyWithError(ctx, "ERR invalid last: must be an unsigned 32 bit integer");
    }
  } else if (argc > 5) {
    return RedisModule_WrongArity(ctx);
  }

  // open destkey for writing
  RedisModuleKey* destkey = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ | REDISMODULE_WRITE);
  int desttype = RedisModule_KeyType(destkey);
  if (desttype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(destkey) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  // checks for srckey types
  RedisModuleKey* srckey = RedisModule_OpenKey(ctx, argv[3], REDISMODULE_READ);
  int srctype = RedisModule_KeyType(srckey);
  if (srctype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(srckey) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  bool should_free = false;
  Bitmap* bitmap = NULL;
  if (srctype == REDISMODULE_KEYTYPE_EMPTY) {
    // "non-existent keys [...] are considered as a stream of zero bytes up to the length of the longest string"
    bitmap = bitmap_alloc();
    should_free = true;
  } else if (RedisModule_ModuleTypeGetType(srckey) == BitmapType) {
    bitmap = RedisModule_ModuleTypeGetValue(srckey);
    should_free = false;
  }

  long long max = -1;
  if (!bitmap_is_empty(bitmap)) {
    max = bitmap_max(bitmap);
  }

  if (argc == 4) {
    last = max;
  } else if (argc == 5 && last < max) {
    last = max;
  }

  // calculate destkey bitmap
  Bitmap* result = bitmap_flip(bitmap, last+1);
  RedisModule_ModuleTypeSetValue(destkey, BitmapType, result);

  if (should_free) {
    bitmap_free(bitmap);
  }

  RedisModule_ReplicateVerbatim(ctx);

  // Integer reply: The size of the string stored in the destination key
  // (adapted to cardinality)
  uint64_t cardinality = bitmap_get_cardinality(result);
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
    return (RedisModule_IsKeysPositionRequest(ctx) > 0) ? REDISMODULE_OK : RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  size_t len;
  const char* operation = RedisModule_StringPtrLen(argv[1], &len);

  if (strcmp(operation, "NOT") == 0) {
    return RBitFlip(ctx, argv, argc);
  } else if (strcmp(operation, "AND") == 0) {
    return RBitOp(ctx, argv, argc, bitmap_and);
  } else if (strcmp(operation, "OR") == 0) {
    return RBitOp(ctx, argv, argc, bitmap_or);
  } else if (strcmp(operation, "XOR") == 0) {
    return RBitOp(ctx, argv, argc, bitmap_xor);
  } else {
    if (RedisModule_IsKeysPositionRequest(ctx) > 0) {
      return REDISMODULE_OK;
    } else {
      RedisModule_ReplyWithSimpleString(ctx, "ERR syntax error");
      return REDISMODULE_ERR;
    }
  }
  return REDISMODULE_OK;
}

/**
 * R.BITCOUNT <key>
 * */
int RBitCountCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  uint64_t count;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    count = 0;
  } else {
    Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
    count = bitmap_get_cardinality(bitmap);
  }

  RedisModule_ReplyWithLongLong(ctx, (long long) count);

  return REDISMODULE_OK;
}


/**
 * R.BITPOS <key> <bit>
 * */
int RBitPosCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  long long bit;
  RedisModule_StringToLongLong(argv[2], &bit);

  int64_t pos;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    pos = -1;
  } else {
    Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
    if (bit == 1) {
      pos = bitmap_get_nth_element_present(bitmap, 1);
    } else {
      pos = bitmap_get_nth_element_not_present(bitmap, 1);
    }
  }

  RedisModule_ReplyWithLongLong(ctx, (long long) pos);

  return REDISMODULE_OK;
}

int RMinCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  long long result;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    result = -1;
  } else {
    Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
    if (bitmap_is_empty(bitmap)) {
      result = -1;
    } else {
      result = bitmap_min(bitmap);
    }
  }

  RedisModule_ReplyWithLongLong(ctx, result);
  return REDISMODULE_OK;
}

int RMaxCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  long long result;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    result = -1;
  } else {
    Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
    if (bitmap_is_empty(bitmap)) {
      result = -1;
    } else {
      result = bitmap_max(bitmap);
    }
  }

  RedisModule_ReplyWithLongLong(ctx, result);
  return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx* ctx) {
  // Register the module itself
  if (RedisModule_Init(ctx, "REDIS-ROARING", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
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

  BitmapType = RedisModule_CreateDataType(ctx, "reroaring", BITMAP_ENCODING_VERSION, &tm);
  if (BitmapType == NULL) return REDISMODULE_ERR;

  // register our commands
  if (RedisModule_CreateCommand(ctx, "R.SETBIT", RSetBitCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.GETBIT", RGetBitCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "R.SETINTARRAY", RSetIntArrayCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.GETINTARRAY", RGetIntArrayCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.RANGEINTARRAY", RRangeIntArrayCommand, "readonly", 1, 1, 1) ==
      REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.APPENDINTARRAY", RAppendIntArrayCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.DIFF", RDiffCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.SETFULL", RSetFullCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.SETRANGE", RSetRangeCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.OPTIMIZE", ROptimizeBitCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.STAT", RStatBitCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.SETBITARRAY", RSetBitArrayCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.GETBITARRAY", RGetBitArrayCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.BITOP", RBitOpCommand, "write getkeys-api", 0, 0, 0) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.BITCOUNT", RBitCountCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.BITPOS", RBitPosCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.MIN", RMinCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.MAX", RMaxCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
