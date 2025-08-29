#include "redismodule.h"
#include "data-structure.h"
#include "hiredis/sds.h"
#include "rmalloc.h"
#include "roaring.h"

static RedisModuleType* BitmapType;
static RedisModuleType* Bitmap64Type;

#define BITMAP_ENCODING_VERSION 1

void ReplyWithUint64(RedisModuleCtx* ctx, uint64_t value) {
  if (value <= INT64_MAX) {
    RedisModule_ReplyWithLongLong(ctx, (long long) value);
  } else {
    char buffer[21];
    size_t len = uint64_to_string(value, buffer);
    RedisModule_ReplyWithBigNumber(ctx, buffer, len);
  }
}

void BitmapRdbSave(RedisModuleIO* rdb, void* value);
void* BitmapRdbLoad(RedisModuleIO* rdb, int encver);
void BitmapAofRewrite(RedisModuleIO* aof, RedisModuleString* key, void* value);
size_t BitmapMemUsage(const void* value);
void BitmapFree(void* value);

/* === Bitmap type methods === */
void BitmapRdbSave(RedisModuleIO* rdb, void* value) {
  Bitmap* bitmap = value;
  size_t serialized_max_size = roaring_bitmap_size_in_bytes(bitmap);
  char* serialized_bitmap = rm_malloc(serialized_max_size);
  size_t serialized_size = roaring_bitmap_serialize(bitmap, serialized_bitmap);
  RedisModule_SaveStringBuffer(rdb, serialized_bitmap, serialized_size);
  rm_free(serialized_bitmap);
}

void* BitmapRdbLoad(RedisModuleIO* rdb, int encver) {
  if (encver != BITMAP_ENCODING_VERSION) {
    RedisModule_LogIOError(rdb, "warning", "Can't load data with version %d", encver);
    return NULL;
  }
  size_t size;
  char* serialized_bitmap = RedisModule_LoadStringBuffer(rdb, &size);
  Bitmap* bitmap = roaring_bitmap_deserialize(serialized_bitmap);
  rm_free(serialized_bitmap);
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

/* === Bitmap64 type methods === */
void Bitmap64RdbSave(RedisModuleIO* rdb, void* value);
void* Bitmap64RdbLoad(RedisModuleIO* rdb, int encver);
void Bitmap64AofRewrite(RedisModuleIO* aof, RedisModuleString* key, void* value);
size_t Bitmap64MemUsage(const void* value);
void Bitmap64Free(void* value);

void Bitmap64RdbSave(RedisModuleIO* rdb, void* value) {
  Bitmap64* bitmap = value;
  size_t serialized_max_size = roaring64_bitmap_portable_size_in_bytes(bitmap);
  char* serialized_bitmap = rm_malloc(serialized_max_size);
  size_t serialized_size = roaring64_bitmap_portable_serialize(bitmap, serialized_bitmap);
  RedisModule_SaveStringBuffer(rdb, serialized_bitmap, serialized_size);
  rm_free(serialized_bitmap);
}

void* Bitmap64RdbLoad(RedisModuleIO* rdb, int encver) {
  if (encver != BITMAP_ENCODING_VERSION) {
    RedisModule_LogIOError(rdb, "warning", "Can't load data with version %d", encver);
    return NULL;
  }
  size_t size;
  char* serialized_bitmap = RedisModule_LoadStringBuffer(rdb, &size);
  Bitmap64* bitmap = roaring64_bitmap_portable_deserialize_safe(serialized_bitmap, size);
  rm_free(serialized_bitmap);
  return bitmap;
}

size_t Bitmap64MemUsage(const void* value) {
  const Bitmap64* bitmap = value;
  return roaring64_bitmap_portable_size_in_bytes(bitmap);
}

void Bitmap64Free(void* value) {
  bitmap64_free(value);
}

void Bitmap64AofRewrite(RedisModuleIO* aof, RedisModuleString* key, void* value) {
  Bitmap64* bitmap = value;

  uint64_t cardinality = bitmap64_get_cardinality(bitmap);
  if (cardinality == 0) {
    return;
  }

  uint64_t* values = rm_malloc(sizeof(uint64_t) * cardinality);
  if (values == NULL) {
    return;
  }

  roaring64_bitmap_to_uint64_array(bitmap, values);

  /* Use R64.SETINTARRAY command to reconstruct the bitmap */
  RedisModule_EmitAOF(aof, "R64.SETINTARRAY", "sl", key, values, cardinality);

  rm_free(values);
}

/* === Bitmap64 commands === */


/**
 * R64.SETBIT <key> <offset> <value>
 * */
int R64SetBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  unsigned long long offset;
  if ((RedisModule_StringToULongLong(argv[2], &offset) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid offset: must be an unsigned 64 bit integer");
  }
  long long value;
  if ((RedisModule_StringToLongLong(argv[3], &value) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be either 0 or 1");
  }

  /* Create an empty value object if the key is currently empty. */
  Bitmap64* bitmap;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    bitmap = bitmap64_alloc();
    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
  } else {
    bitmap = RedisModule_ModuleTypeGetValue(key);
  }

  /* Set bit with value */
  char old_value = bitmap64_getbit(bitmap, (uint64_t) offset);
  bitmap64_setbit(bitmap, (uint64_t) offset, (char) value);

  RedisModule_ReplicateVerbatim(ctx);
  // Integer reply: the original bit value stored at offset.
  RedisModule_ReplyWithLongLong(ctx, old_value);

  return REDISMODULE_OK;
}

/**
 * R64.GETBIT <key> <offset>
 * */
int R64GetBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  unsigned long long offset;
  if ((RedisModule_StringToULongLong(argv[2], &offset) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid offset: must be an unsigned 64 bit integer");
  }

  char value;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    value = 0;
  } else {
    Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
    /* Get bit */
    value = bitmap64_getbit(bitmap, (uint64_t) offset);
  }

  RedisModule_ReplyWithLongLong(ctx, value);

  return REDISMODULE_OK;
}

/**
 * R64.SETINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int R64SetIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  size_t length = (size_t) (argc - 2);
  uint64_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    unsigned long long value;
    if ((RedisModule_StringToULongLong(argv[2 + i], &value) != REDISMODULE_OK)) {
      rm_free(values);
      return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be an unsigned 64 bit integer");
    }
    values[i] = (uint64_t) value;
  }

  Bitmap64* bitmap = bitmap64_from_int_array(length, values);
  RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);

  rm_free(values);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");


  return REDISMODULE_OK;
}

/**
 * R64.GETINTARRAY <key>
 * */
int R64GetIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap64* bitmap = (type == REDISMODULE_KEYTYPE_EMPTY) ? NULL : RedisModule_ModuleTypeGetValue(key);

  uint64_t n = 0;
  uint64_t* array = bitmap == NULL ? NULL : bitmap64_get_int_array(bitmap, &n);

  RedisModule_ReplyWithArray(ctx, n);

  for (uint64_t i = 0; i < n; i++) {
    ReplyWithUint64(ctx, array[i]);
  }

  rm_free(array);

  return REDISMODULE_OK;
}

/**
 * R64.RANGEINTARRAY <key> <start> <end>
 * */
int R64RangeIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  unsigned long long start;
  if ((RedisModule_StringToULongLong(argv[2], &start) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid start: must be an unsigned 64 bit integer");
  }

  unsigned long long end;
  if ((RedisModule_StringToULongLong(argv[3], &end) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid start: must be an unsigned 64 bit integer");
  }

  if (start < 0 || end < 0) {
    return RedisModule_ReplyWithError(ctx, "ERR start and end must be >= 0");
  }

  Bitmap64* bitmap = (type == REDISMODULE_KEYTYPE_EMPTY) ? NULL : RedisModule_ModuleTypeGetValue(key);
  uint64_t* array = NULL;
  uint64_t n = 0;
  if (bitmap != NULL) {
    unsigned long long count = (unsigned long long) bitmap64_get_cardinality(bitmap);

    if (start > count - 1 || start > end) {
      n = 0;
    } else {
      n = end - start + 1;
    }
    if (n > 0) {
      array = bitmap64_range_int_array(bitmap, start, n);
    }
  } else {
    n = 0;
  }
  RedisModule_ReplyWithArray(ctx, n);
  for (size_t i = 0; i < n; i++) {
    ReplyWithUint64(ctx, array[i]);
  }
  if (array != NULL) {
    rm_free(array);
  }
  return REDISMODULE_OK;
}

/**
 * R64.APPENDINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int R64AppendIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap64* bitmap = (type == REDISMODULE_KEYTYPE_EMPTY) ? NULL : RedisModule_ModuleTypeGetValue(key);
  bool bitmap_created = false;

  if (bitmap == NULL) {
    bitmap = bitmap64_alloc();
    bitmap_created = true;
  }

  size_t length = (size_t) (argc - 2);
  uint64_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    unsigned long long value;
    if ((RedisModule_StringToULongLong(argv[2 + i], &value) != REDISMODULE_OK)) {
      rm_free(values);
      return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be an unsigned 64 bit integer");
    }
    values[i] = (uint64_t) value;
  }

  roaring64_bitmap_add_many(bitmap, length, values);

  if (bitmap_created) {
    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
  } else {
    RedisModule_SignalModifiedKey(ctx, argv[1]);
  }

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  rm_free(values);

  return REDISMODULE_OK;
}

/**
 * R64.DELETEINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int R64DeleteIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap64* bitmap = (type == REDISMODULE_KEYTYPE_EMPTY) ? NULL : RedisModule_ModuleTypeGetValue(key);

  if (bitmap == NULL) {
    bitmap = bitmap64_alloc();

    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");

    return REDISMODULE_OK;
  }

  size_t length = (size_t) (argc - 2);
  uint64_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    unsigned long long value;
    if ((RedisModule_StringToULongLong(argv[2 + i], &value) != REDISMODULE_OK)) {
      rm_free(values);
      return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be an unsigned 64 bit integer");
    }
    values[i] = (uint64_t) value;
  }

  roaring64_bitmap_remove_many(bitmap, length, values);

  RedisModule_SignalModifiedKey(ctx, argv[1]);
  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  rm_free(values);

  return REDISMODULE_OK;
}

/**
 * R64.DIFF <dest> <decreasing> <deductible>
 * */
int R64DiffCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);

  // open destkey for writing
  RedisModuleKey* destkey = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int desttype = RedisModule_KeyType(destkey);
  if (desttype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(destkey) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  // checks for keys types
  for (uint32_t i = 2; i < 4; i++) {
    RedisModuleKey* srckey = RedisModule_OpenKey(ctx, argv[i], REDISMODULE_READ);
    int srctype = RedisModule_KeyType(srckey);
    if (srctype == REDISMODULE_KEYTYPE_EMPTY || RedisModule_ModuleTypeGetType(srckey) != Bitmap64Type) {
      return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }
  }

  RedisModuleKey* decreasing = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ);
  Bitmap64* decreasing_bitmap = RedisModule_ModuleTypeGetValue(decreasing);
  RedisModuleKey* deductible = RedisModule_OpenKey(ctx, argv[3], REDISMODULE_READ);
  Bitmap64* deductible_bitmap = RedisModule_ModuleTypeGetValue(deductible);

  Bitmap64* result = roaring64_bitmap_andnot(decreasing_bitmap, deductible_bitmap);
  RedisModule_ModuleTypeSetValue(destkey, Bitmap64Type, result);
  RedisModule_ReplyWithSimpleString(ctx, "OK");
  return REDISMODULE_OK;
}

/**
 * R64.SETFULL <key>
 * */
int R64SetFullCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithError(ctx, "key exists");
  }

  Bitmap64* bitmap = bitmap64_from_range(0, UINT64_MAX);
  //NOTE bitmap from range is an right open interval, to set full bit for key, we need do it manually
  bitmap64_setbit(bitmap, UINT64_MAX, 1);
  RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");
  return REDISMODULE_OK;
}


/**
 * R64.SETRANGE <key> <start_num> <end_num>
 * */
int R64SetRangeCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  unsigned long long start_num;
  if ((RedisModule_StringToULongLong(argv[2], &start_num) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid start_num: must be an unsigned 64 bit integer");
  }
  unsigned long long end_num;
  if ((RedisModule_StringToULongLong(argv[3], &end_num) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid end_num: must be an unsigned 64 bit integer");
  }
  if (start_num < 0 || end_num < 0 || end_num < start_num) {
    return RedisModule_ReplyWithError(ctx, "start_num and end_num must >=0 and end_num must >= start_num");
  }

  if ((uint64_t) start_num > UINT64_MAX || (uint64_t) end_num > UINT64_MAX) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid end_num: must be an unsigned 64 bit integer");
  }
  Bitmap64* bitmap = (type == REDISMODULE_KEYTYPE_EMPTY) ? NULL : RedisModule_ModuleTypeGetValue(key);

  Bitmap64* nbitmap = bitmap64_from_range((uint64_t) start_num, (uint64_t) end_num);

  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, nbitmap);
  } else {
    Bitmap64** bitmaps = rm_malloc(2 * sizeof(*bitmaps));
    bitmaps[0] = bitmap;
    bitmaps[1] = nbitmap;
    Bitmap64* result = bitmap64_or(2, (const Bitmap64**) bitmaps);
    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, result);
    bitmap64_free(bitmaps[1]);
    rm_free(bitmaps);
  }

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");
  return REDISMODULE_OK;
}

/**
 * R64.OPTIMIZE <key> [--mem]
 * */
int R64OptimizeBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  int shrink_to_fit = 0;
  if (argc < 2 || argc > 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithError(ctx, "ERR no such key");
  }
  if (RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  if (argc == 3) {
    size_t len;
    const char* option = RedisModule_StringPtrLen(argv[2], &len);
    if (strcmp(option, "--mem") == 0) {
      shrink_to_fit = 1;
    }
  }

  Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
  bitmap64_optimize(bitmap, shrink_to_fit);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}


/**
 * R64.STAT <key>
 * */
int R64StatBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithError(ctx, "ERR key does not exist");
  }

  Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
  Bitmap64_statistics stat;
  bitmap64_statistics(bitmap, &stat);

  sds s = sdsempty();
  s = sdscatprintf(s, "cardinality: %lld\n", (long long) stat.cardinality);
  s = sdscatprintf(s, "number of containers: %llu\n", stat.n_containers);
  s = sdscatprintf(s, "max value: %llu\n", stat.max_value);
  s = sdscatprintf(s, "min value: %llu\n", stat.min_value);

  s = sdscatprintf(s, "number of array containers: %llu\n", stat.n_array_containers);
  s = sdscatprintf(s, "\tarray container values: %llu\n", stat.n_values_array_containers);
  s = sdscatprintf(s, "\tarray container bytes: %llu\n", stat.n_bytes_array_containers);

  s = sdscatprintf(s, "bitset  containers: %llu\n", stat.n_bitset_containers);
  s = sdscatprintf(s, "\tbitset  container values: %llu\n", stat.n_values_bitset_containers);
  s = sdscatprintf(s, "\tbitset  container bytes: %llu\n", stat.n_bytes_bitset_containers);

  s = sdscatprintf(s, "run containers: %llu\n", stat.n_run_containers);
  s = sdscatprintf(s, "\trun container values: %llu\n", stat.n_values_run_containers);
  s = sdscatprintf(s, "\trun container bytes: %llu\n", stat.n_bytes_run_containers);

  RedisModule_ReplyWithVerbatimString(ctx, s, sdslen(s));
  sdsfree(s);

  return REDISMODULE_OK;
}

/**
 * R64.SETBITARRAY <key> <value1>
 * */
int R64SetBitArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  size_t len;
  const char* array = RedisModule_StringPtrLen(argv[2], &len);
  Bitmap64* bitmap = bitmap64_from_bit_array(len, array);
  RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}

/**
 * R64.GETBITARRAY <key>
 * */
int R64GetBitArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  if (type != REDISMODULE_KEYTYPE_EMPTY) {
    Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
    uint64_t size;
    char* array = bitmap64_get_bit_array(bitmap, &size);
    RedisModule_ReplyWithStringBuffer(ctx, array, size);

    bitmap_free_bit_array(array);
  } else {
    RedisModule_ReplyWithSimpleString(ctx, "");
  }

  return REDISMODULE_OK;
}

int R64BitFlip(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (RedisModule_IsKeysPositionRequest(ctx) > 0) {
    if (argc <= 5) {
      RedisModule_KeyAtPos(ctx, 2);
      RedisModule_KeyAtPos(ctx, 3);
    }
    return REDISMODULE_OK;
  }

  unsigned long long last = 0;

  if (argc == 5) {
    if ((RedisModule_StringToULongLong(argv[4], &last) != REDISMODULE_OK)) {
      return RedisModule_ReplyWithError(ctx, "ERR invalid last: must be an unsigned 64 bit integer");
    }
  } else if (argc > 5) {
    return RedisModule_WrongArity(ctx);
  }

  // open destkey for writing
  RedisModuleKey* destkey = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ | REDISMODULE_WRITE);
  int desttype = RedisModule_KeyType(destkey);
  if (desttype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(destkey) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  // checks for srckey types
  RedisModuleKey* srckey = RedisModule_OpenKey(ctx, argv[3], REDISMODULE_READ);
  int srctype = RedisModule_KeyType(srckey);
  if (srctype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(srckey) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  bool should_free = false;
  Bitmap64* bitmap = NULL;
  if (srctype == REDISMODULE_KEYTYPE_EMPTY) {
    // "non-existent keys [...] are considered as a stream of zero bytes up to the length of the longest string"
    bitmap = bitmap64_alloc();
    should_free = true;
  } else if (RedisModule_ModuleTypeGetType(srckey) == Bitmap64Type) {
    bitmap = RedisModule_ModuleTypeGetValue(srckey);
    should_free = false;
  }

  unsigned long long max = 0;
  if (!bitmap64_is_empty(bitmap)) {
    max = (unsigned long long) bitmap64_max(bitmap);
  }

  if (argc == 4) {
    last = max;
  } else if (argc == 5 && last < max) {
    last = max;
  }

  // calculate destkey bitmap
  Bitmap64* result = bitmap64_flip(bitmap, last);
  RedisModule_ModuleTypeSetValue(destkey, Bitmap64Type, result);

  if (should_free) {
    bitmap64_free(bitmap);
  }

  RedisModule_ReplicateVerbatim(ctx);

  // Integer reply: The size of the string stored in the destination key
  // (adapted to cardinality)
  uint64_t cardinality = bitmap64_get_cardinality(result);
  ReplyWithUint64(ctx, cardinality);

  return REDISMODULE_OK;
}

int R64BitOp(RedisModuleCtx* ctx, RedisModuleString** argv, int argc, Bitmap64* (*operation)(uint32_t, const Bitmap64**)) {
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
  if (desttype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(destkey) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  uint32_t n = (uint32_t) (argc - 3);
  // checks for srckey types
  for (uint32_t i = 0; i < n; i++) {
    RedisModuleKey* srckey = RedisModule_OpenKey(ctx, argv[3 + i], REDISMODULE_READ);
    int srctype = RedisModule_KeyType(srckey);
    if (srctype != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(srckey) != Bitmap64Type) {
      return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }
  }

  // alloc memory for srckey bitmaps
  Bitmap64** bitmaps = rm_malloc(n * sizeof(*bitmaps));
  bool* should_free = rm_malloc(n * sizeof(*should_free));

  for (uint32_t i = 0; i < n; i++) {
    RedisModuleKey* srckey = RedisModule_OpenKey(ctx, argv[3 + i], REDISMODULE_READ);
    int srctype = RedisModule_KeyType(srckey);

    Bitmap64* bitmap;
    if (srctype == REDISMODULE_KEYTYPE_EMPTY) {
      // "non-existent keys [...] are considered as a stream of zero bytes up to the length of the longest string"
      bitmap = bitmap64_alloc();
      should_free[i] = true;
    } else {
      bitmap = RedisModule_ModuleTypeGetValue(srckey);
      should_free[i] = false;
    }
    bitmaps[i] = bitmap;
  }

  // calculate destkey bitmap
  Bitmap64* result = operation(n, (const Bitmap64**) bitmaps);
  RedisModule_ModuleTypeSetValue(destkey, Bitmap64Type, result);

  for (uint32_t i = 0; i < n; i++) {
    if (should_free[i]) {
      bitmap64_free(bitmaps[i]);
    }
  }
  rm_free(should_free);
  rm_free(bitmaps);

  RedisModule_ReplicateVerbatim(ctx);

  // Integer reply: The size of the string stored in the destination key
  // (adapted to cardinality)
  uint64_t cardinality = bitmap64_get_cardinality(result);
  ReplyWithUint64(ctx, cardinality);

  return REDISMODULE_OK;
}

/**
 * BITOP AND destkey srckey1 srckey2 srckey3 ... srckeyN
 * BITOP OR destkey srckey1 srckey2 srckey3 ... srckeyN
 * BITOP XOR destkey srckey1 srckey2 srckey3 ... srckeyN
 * BITOP NOT destkey srckey
 * R64.BITOP <operation> <destkey> <key> [<key> ...]
 * */
int R64BitOpCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 4) {
    return (RedisModule_IsKeysPositionRequest(ctx) > 0) ? REDISMODULE_OK : RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  size_t len;
  const char* operation = RedisModule_StringPtrLen(argv[1], &len);

  if (strcmp(operation, "NOT") == 0) {
    return R64BitFlip(ctx, argv, argc);
  } else if (strcmp(operation, "AND") == 0) {
    return R64BitOp(ctx, argv, argc, bitmap64_and);
  } else if (strcmp(operation, "OR") == 0) {
    return R64BitOp(ctx, argv, argc, bitmap64_or);
  } else if (strcmp(operation, "XOR") == 0) {
    return R64BitOp(ctx, argv, argc, bitmap64_xor);
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
 * R64.BITCOUNT <key>
 * */
int R64BitCountCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  uint64_t count;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    count = 0;
  } else {
    Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
    count = bitmap64_get_cardinality(bitmap);
  }

  ReplyWithUint64(ctx, count);

  return REDISMODULE_OK;
}

/**
 * R64.BITPOS <key> <bit>
 * */
int R64BitPosCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  long long bit;
  RedisModule_StringToLongLong(argv[2], &bit);

  bool found = false;
  uint64_t pos;
  if (type != REDISMODULE_KEYTYPE_EMPTY) {
    Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
    if (bit == 1) {
      pos = bitmap64_get_nth_element_present(bitmap, 1, &found);
    } else {
      pos = bitmap64_get_nth_element_not_present(bitmap, 1, &found);
      found = true;
    }
  }

  if (found) {
    ReplyWithUint64(ctx, pos);
  } else {
    RedisModule_ReplyWithLongLong(ctx, -1);
  }

  return REDISMODULE_OK;
}

int R64MinCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  uint64_t result;
  bool found = false;

  if (type != REDISMODULE_KEYTYPE_EMPTY) {
    Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
    if (!bitmap64_is_empty(bitmap)) {
      result = bitmap64_min(bitmap);
      found = true;
    }
  }

  if (found) {
    ReplyWithUint64(ctx, result);
  } else {
    RedisModule_ReplyWithLongLong(ctx, -1);
  }

  return REDISMODULE_OK;
}

int R64MaxCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  int type = RedisModule_KeyType(key);
  if (type != REDISMODULE_KEYTYPE_EMPTY && RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  uint64_t result;
  bool found = false;

  if (type != REDISMODULE_KEYTYPE_EMPTY) {
    Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
    if (!bitmap64_is_empty(bitmap)) {
      result = bitmap64_max(bitmap);
      found = true;
    }
  }

  if (found) {
    ReplyWithUint64(ctx, result);
  } else {
    RedisModule_ReplyWithLongLong(ctx, -1);
  }

  return REDISMODULE_OK;
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
    Bitmap** bitmaps = rm_malloc(2 * sizeof(*bitmaps));
    bitmaps[0] = bitmap;
    bitmaps[1] = nbitmap;
    Bitmap* result = bitmap_or(2, (const Bitmap**) bitmaps);
    RedisModule_ModuleTypeSetValue(key, BitmapType, result);
    bitmap_free(bitmaps[1]);
    rm_free(bitmaps);
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
 * R.OPTIMIZE <key> [--mem]
 * */
int ROptimizeBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  int shrink_to_fit = 0;
  if (argc < 2 || argc > 3) {
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

  if (argc == 3) {
    size_t len;
    const char* option = RedisModule_StringPtrLen(argv[2], &len);
    if (strcmp(option, "--mem") == 0) {
      shrink_to_fit = 1;
    }
  }

  Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
  bitmap_optimize(bitmap, shrink_to_fit);

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

  size_t length = (size_t) (argc - 2);
  uint32_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    long long value;
    if ((RedisModule_StringToLongLong(argv[2 + i], &value) != REDISMODULE_OK)) {
      rm_free(values);
      return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be an unsigned 32 bit integer");
    }
    values[i] = (uint32_t) value;
  }

  Bitmap* bitmap = bitmap_from_int_array(length, values);
  RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);

  rm_free(values);

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
  bool bitmap_created = false;

  if (bitmap == NULL) {
    bitmap = bitmap_alloc();
    bitmap_created = true;
  }

  size_t length = (size_t) (argc - 2);
  uint32_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    long long value;
    if ((RedisModule_StringToLongLong(argv[2 + i], &value) != REDISMODULE_OK)) {
      rm_free(values);
      return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be an unsigned 32 bit integer");
    }
    values[i] = (uint32_t) value;
  }

  roaring_bitmap_add_many(bitmap, length, values);

  if (bitmap_created) {
    RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
  } else {
    RedisModule_SignalModifiedKey(ctx, argv[1]);
  }

  rm_free(values);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}

/**
 * R.DELETEINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int RDeleteIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
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

  if (bitmap == NULL) {
    bitmap = bitmap_alloc();

    RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");

    return REDISMODULE_OK;
  }

  size_t length = (size_t) (argc - 2);
  uint32_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    long long value;
    if ((RedisModule_StringToLongLong(argv[2 + i], &value) != REDISMODULE_OK)) {
      rm_free(values);
      return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be an unsigned 32 bit integer");
    }
    values[i] = (uint32_t) value;
  }

  roaring_bitmap_remove_many(bitmap, length, values);

  RedisModule_SignalModifiedKey(ctx, argv[1]);
  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");

  rm_free(values);

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
    rm_free(array);
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

  rm_free(array);

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


int RBitOp(RedisModuleCtx* ctx, RedisModuleString** argv, int argc, Bitmap* (*operation)(uint32_t, const Bitmap**)) {
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
  Bitmap** bitmaps = rm_malloc(n * sizeof(*bitmaps));
  bool* should_free = rm_malloc(n * sizeof(*should_free));

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
  rm_free(should_free);
  rm_free(bitmaps);

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
  Bitmap* result = bitmap_flip(bitmap, last + 1);
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

  RedisModuleTypeMethods tm32 = {
      .version = REDISMODULE_TYPE_METHOD_VERSION,
      .rdb_load = BitmapRdbLoad,
      .rdb_save = BitmapRdbSave,
      .aof_rewrite = BitmapAofRewrite,
      .mem_usage = BitmapMemUsage,
      .free = BitmapFree
  };

  BitmapType = RedisModule_CreateDataType(ctx, "reroaring", BITMAP_ENCODING_VERSION, &tm32);
  if (BitmapType == NULL) return REDISMODULE_ERR;

  RedisModuleTypeMethods tm64 = {
      .version = REDISMODULE_TYPE_METHOD_VERSION,
      .rdb_load = Bitmap64RdbLoad,
      .rdb_save = Bitmap64RdbSave,
      .aof_rewrite = Bitmap64AofRewrite,
      .mem_usage = Bitmap64MemUsage,
      .free = Bitmap64Free
  };

  Bitmap64Type = RedisModule_CreateDataType(ctx, "roaring64", BITMAP_ENCODING_VERSION, &tm64);
  if (Bitmap64Type == NULL) {
    RedisModule_Log(ctx, "warning", "Failed to register the Bitmap64Type data type");
    return REDISMODULE_ERR;
  }

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
  if (RedisModule_CreateCommand(ctx, "R.RANGEINTARRAY", RRangeIntArrayCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.APPENDINTARRAY", RAppendIntArrayCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R.DELETEINTARRAY", RDeleteIntArrayCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
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

  if (RedisModule_CreateCommand(ctx, "R64.SETBIT", R64SetBitCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.GETBIT", R64GetBitCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.SETINTARRAY", R64SetIntArrayCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.GETINTARRAY", R64GetIntArrayCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.RANGEINTARRAY", R64RangeIntArrayCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.APPENDINTARRAY", R64AppendIntArrayCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.DELETEINTARRAY", R64DeleteIntArrayCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.DIFF", R64DiffCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.SETFULL", R64SetFullCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.SETRANGE", R64SetRangeCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.OPTIMIZE", R64OptimizeBitCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.STAT", R64StatBitCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.SETBITARRAY", R64SetBitArrayCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.GETBITARRAY", R64GetBitArrayCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.BITOP", R64BitOpCommand, "write getkeys-api", 0, 0, 0) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.BITCOUNT", R64BitCountCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.BITPOS", R64BitPosCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.MIN", R64MinCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.MAX", R64MaxCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
