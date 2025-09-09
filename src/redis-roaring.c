#include "redismodule.h"
#include "data-structure.h"
#include "hiredis/sds.h"
#include "rmalloc.h"
#include "roaring.h"

static RedisModuleType* BitmapType;
static RedisModuleType* Bitmap64Type;
static Bitmap64* BITMAP64_NILL;
static Bitmap* BITMAP_NILL;
static char REPLY_UINT64_BUFFER[21];

#define BITMAP_ENCODING_VERSION 1
#define BITMAP_MAX_RANGE_SIZE 100000000

void ReplyWithUint64(RedisModuleCtx* ctx, uint64_t value) {
  size_t len = uint64_to_string(value, REPLY_UINT64_BUFFER);
  RedisModule_ReplyWithBigNumber(ctx, REPLY_UINT64_BUFFER, len);
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
 * R64.GETBITS <key> offset [offset1 offset2 ... offsetN]
 * */
int R64GetBitManyCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  if (RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);

  size_t n_offsets = (size_t) (argc - 2);
  uint64_t* offsets = rm_malloc(sizeof(*offsets) * n_offsets);

  for (int i = 0; i < n_offsets; i++) {
    unsigned long long value;
    if ((RedisModule_StringToULongLong(argv[2 + i], &value) != REDISMODULE_OK) || value > UINT64_MAX) {
      rm_free(offsets);
      return RedisModule_ReplyWithError(ctx, "ERR invalid offset: must be an unsigned 64 bit integer");
    }
    offsets[i] = (uint64_t) value;
  }

  bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);

  RedisModule_ReplyWithArray(ctx, n_offsets);

  for (size_t i = 0; i < n_offsets; i++) {
    RedisModule_ReplyWithLongLong(ctx, (long long) results[i]);
  }

  rm_free(offsets);
  rm_free(results);

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

  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  if (RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  unsigned long long start;
  if ((RedisModule_StringToULongLong(argv[2], &start) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid start: must be an unsigned 64 bit integer");
  }

  unsigned long long end;
  if ((RedisModule_StringToULongLong(argv[3], &end) != REDISMODULE_OK)) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid end: must be an unsigned 64 bit integer");
  }

  if (start > end) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  // Check for potential overflow in range calculation
  uint64_t range_size = (end - start) + 1;

  if (range_size > BITMAP_MAX_RANGE_SIZE) {
    return RedisModule_ReplyWithErrorFormat(ctx, "ERR range too large: maximum %llu elements", BITMAP_MAX_RANGE_SIZE);
  }

  uint64_t count;
  Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);
  uint64_t* array = bitmap64_range_int_array(bitmap, start, end, &count);

  if (array == NULL) {
    return RedisModule_ReplyWithError(ctx, "ERR out of memory");
  }

  if (count == 0) {
    rm_free(array);
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  RedisModule_ReplyWithArray(ctx, (long) count);

  for (uint64_t i = 0; i < count; i++) {
    ReplyWithUint64(ctx, array[i]);
  }

  rm_free(array);
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

  if (bitmap == NULL) {
    bitmap = bitmap64_from_range(start_num, end_num);
    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
  } else {
    roaring64_bitmap_add_range(bitmap, start_num, end_num);
    RedisModule_SignalModifiedKey(ctx, argv[1]);
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

int R64BitOp(RedisModuleCtx* ctx, RedisModuleString** argv, int argc, void (*operation)(Bitmap64*, uint32_t, const Bitmap64**)) {
  if (RedisModule_IsKeysPositionRequest(ctx) > 0) {
    if (argc > 4) {
      for (int i = 2; i < argc; i++) {
        RedisModule_KeyAtPos(ctx, i);
      }
    }
    return REDISMODULE_OK;
  }

  if (argc == 4) return RedisModule_WrongArity(ctx);

  uint32_t n = ((uint32_t) (argc - 2));

  // alloc memory for srckey bitmaps
  Bitmap64** bitmaps = rm_malloc(n * sizeof(*bitmaps));
  RedisModuleKey** srckeys = rm_malloc(n * sizeof(RedisModuleKey*));

  // open destkey for writing
  bool overwrite_dest_key = false;

  srckeys[0] = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ | REDISMODULE_WRITE);

  if (RedisModule_KeyType(srckeys[0]) == REDISMODULE_KEYTYPE_EMPTY ||
    RedisModule_ModuleTypeGetType(srckeys[0]) != Bitmap64Type) {
    bitmaps[0] = bitmap64_alloc();
    overwrite_dest_key = true;
  } else {
    bitmaps[0] = RedisModule_ModuleTypeGetValue(srckeys[0]);
  }

  // checks for srckey types
  for (uint32_t i = 1; i < n; i++) {
    srckeys[i] = RedisModule_OpenKey(ctx, argv[2 + i], REDISMODULE_READ);

    if (RedisModule_KeyType(srckeys[i]) == REDISMODULE_KEYTYPE_EMPTY) {
      bitmaps[i] = BITMAP64_NILL;
    } else if (RedisModule_ModuleTypeGetType(srckeys[i]) != Bitmap64Type) {
      rm_free(bitmaps);
      rm_free(srckeys);
      return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    } else {
      bitmaps[i] = RedisModule_ModuleTypeGetValue(srckeys[i]);
    }
  }

  // calculate destkey bitmap
  operation(bitmaps[0], n - 1, (const Bitmap64**) (bitmaps + 1));

  if (overwrite_dest_key) {
    RedisModule_ModuleTypeSetValue(srckeys[0], Bitmap64Type, bitmaps[0]);
  } else {
    RedisModule_SignalModifiedKey(ctx, argv[2]);
  }

  RedisModule_ReplicateVerbatim(ctx);

  // Integer reply: The size of the string stored in the destination key
  // (adapted to cardinality)
  uint64_t cardinality = bitmap64_get_cardinality(bitmaps[0]);
  ReplyWithUint64(ctx, cardinality);

  rm_free(bitmaps);
  rm_free(srckeys);

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
  } else if (strcmp(operation, "ANDOR") == 0) {
    return R64BitOp(ctx, argv, argc, bitmap64_andor);
  } else if (strcmp(operation, "ONE") == 0) {
    return R64BitOp(ctx, argv, argc, bitmap64_one);
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

/**
 * R64.CLEAR <key>
 * */
int R64ClearCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);

  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithNull(ctx);
  }

  if (RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);

  uint64_t count = bitmap64_get_cardinality(bitmap);

  if (count > 0) {
    roaring64_bitmap_clear(bitmap);
  }

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithLongLong(ctx, (long long) count);

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

  if (bitmap == NULL) {
    bitmap = bitmap_from_range(start_num, end_num);
    RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
  } else {
    roaring_bitmap_add_range(bitmap, start_num, end_num);
    RedisModule_SignalModifiedKey(ctx, argv[1]);
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
 * R.GETBITS <key> offset [offset1 offset2 ... offsetN]
 * */
int RGetBitManyCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  if (RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);

  size_t n_offsets = (size_t) (argc - 2);
  uint32_t* offsets = rm_malloc(sizeof(*offsets) * n_offsets);

  for (int i = 0; i < n_offsets; i++) {
    long long value;
    if ((RedisModule_StringToLongLong(argv[2 + i], &value) != REDISMODULE_OK) || value < 0 || value > UINT32_MAX) {
      rm_free(offsets);
      return RedisModule_ReplyWithError(ctx, "ERR invalid offset: must be an unsigned 32 bit integer");
    }
    offsets[i] = (uint32_t) value;
  }

  bool* results = bitmap_getbits(bitmap, n_offsets, offsets);

  RedisModule_ReplyWithArray(ctx, n_offsets);

  for (size_t i = 0; i < n_offsets; i++) {
    RedisModule_ReplyWithLongLong(ctx, (long long) results[i]);
  }

  rm_free(offsets);
  rm_free(results);

  return REDISMODULE_OK;
}

/**
 * R.STAT <key> [format]
 * */
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

  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  if (RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  long long start;
  if ((RedisModule_StringToLongLong(argv[2], &start) != REDISMODULE_OK) || start < 0 || start > UINT32_MAX) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid start: must be an unsigned 32 bit integer");
  }

  long long end;
  if ((RedisModule_StringToLongLong(argv[3], &end) != REDISMODULE_OK) || end < 0 || end > UINT32_MAX) {
    return RedisModule_ReplyWithError(ctx, "ERR invalid end: must be an unsigned 32 bit integer");
  }

  if (start > end) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  // Check for potential overflow in range calculation
  uint32_t range_size = (end - start) + 1;

  if (range_size > BITMAP_MAX_RANGE_SIZE) {
    return RedisModule_ReplyWithErrorFormat(ctx, "ERR range too large: maximum %llu elements", BITMAP_MAX_RANGE_SIZE);
  }

  size_t count;
  Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);
  uint32_t* array = bitmap_range_int_array(bitmap, start, end, &count);

  if (array == NULL) {
    return RedisModule_ReplyWithError(ctx, "ERR out of memory");
  }

  if (count == 0) {
    rm_free(array);
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  RedisModule_ReplyWithArray(ctx, (long) count);

  for (uint64_t i = 0; i < count; i++) {
    RedisModule_ReplyWithLongLong(ctx, (long long) array[i]);
  }

  rm_free(array);
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

int RBitOp(RedisModuleCtx* ctx, RedisModuleString** argv, int argc, void (*operation)(Bitmap*, uint32_t, const Bitmap**)) {
  if (RedisModule_IsKeysPositionRequest(ctx) > 0) {
    if (argc > 4) {
      for (int i = 2; i < argc; i++) {
        RedisModule_KeyAtPos(ctx, i);
      }
    }
    return REDISMODULE_OK;
  }

  if (argc == 4) return RedisModule_WrongArity(ctx);

  uint32_t n = ((uint32_t) (argc - 2));

  // alloc memory for srckey bitmaps
  Bitmap** bitmaps = rm_malloc(n * sizeof(*bitmaps));
  RedisModuleKey** srckeys = rm_malloc(n * sizeof(RedisModuleKey*));

  // open destkey for writing
  bool overwrite_dest_key = false;

  srckeys[0] = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ | REDISMODULE_WRITE);

  if (RedisModule_KeyType(srckeys[0]) == REDISMODULE_KEYTYPE_EMPTY
    || RedisModule_ModuleTypeGetType(srckeys[0]) != BitmapType) {
    bitmaps[0] = bitmap_alloc();
    overwrite_dest_key = true;
  } else {
    bitmaps[0] = RedisModule_ModuleTypeGetValue(srckeys[0]);
  }

  // checks for srckey types
  for (uint32_t i = 1; i < n; i++) {
    srckeys[i] = RedisModule_OpenKey(ctx, argv[2 + i], REDISMODULE_READ);

    if (RedisModule_KeyType(srckeys[i]) == REDISMODULE_KEYTYPE_EMPTY) {
      bitmaps[i] = BITMAP_NILL;
    } else if (RedisModule_ModuleTypeGetType(srckeys[i]) != BitmapType) {
      rm_free(bitmaps);
      rm_free(srckeys);
      return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    } else {
      bitmaps[i] = RedisModule_ModuleTypeGetValue(srckeys[i]);
    }
  }

  // calculate destkey bitmap
  operation(bitmaps[0], n - 1, (const Bitmap**) (bitmaps + 1));

  if (overwrite_dest_key) {
    RedisModule_ModuleTypeSetValue(srckeys[0], BitmapType, bitmaps[0]);
  } else {
    RedisModule_SignalModifiedKey(ctx, argv[2]);
  }

  RedisModule_ReplicateVerbatim(ctx);

  // Integer reply: The size of the string stored in the destination key
  // (adapted to cardinality)
  uint64_t cardinality = bitmap_get_cardinality(bitmaps[0]);
  ReplyWithUint64(ctx, cardinality);

  rm_free(bitmaps);
  rm_free(srckeys);

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
  } else if (strcmp(operation, "ANDOR") == 0) {
    return RBitOp(ctx, argv, argc, bitmap_andor);
  } else if (strcmp(operation, "ONE") == 0) {
    return RBitOp(ctx, argv, argc, bitmap_one);
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

/**
 * R.CLEAR <key>
 * */
int RClearCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 2) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);

  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithNull(ctx);
  }

  if (RedisModule_ModuleTypeGetType(key) != BitmapType) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap* bitmap = RedisModule_ModuleTypeGetValue(key);

  uint64_t count = bitmap_get_cardinality(bitmap);

  if (count > 0) {
    roaring_bitmap_clear(bitmap);
  }

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithLongLong(ctx, (long long) count);

  return REDISMODULE_OK;
}

void RedisModule_OnShutdown(RedisModuleCtx* ctx, RedisModuleEvent e, uint64_t sub, void* data) {
  REDISMODULE_NOT_USED(e);
  REDISMODULE_NOT_USED(data);
  REDISMODULE_NOT_USED(sub);

  bitmap64_free(BITMAP64_NILL);
  bitmap_free(BITMAP_NILL);
}

int RedisModule_OnLoad(RedisModuleCtx* ctx) {
  // Register the module itself
  if (RedisModule_Init(ctx, "REDIS-ROARING", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  RedisModule_SubscribeToServerEvent(ctx, RedisModuleEvent_Shutdown, RedisModule_OnShutdown);

  BITMAP64_NILL = bitmap64_alloc();
  BITMAP_NILL = bitmap_alloc();

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
  if (RedisModule_CreateCommand(ctx, "R.GETBITS", RGetBitManyCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
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
  if (RedisModule_CreateCommand(ctx, "R.CLEAR", RClearCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.SETBIT", R64SetBitCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.GETBIT", R64GetBitCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }
  if (RedisModule_CreateCommand(ctx, "R64.GETBITS", R64GetBitManyCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
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
  if (RedisModule_CreateCommand(ctx, "R64.CLEAR", R64ClearCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
