#include "r_64.h"
#include "hiredis/sds.h"
#include "rmalloc.h"
#include "roaring.h"
#include "common.h"
#include "parse.h"
#include "cmd_info/command_info.h"

RedisModuleType* Bitmap64Type = NULL;
Bitmap64* BITMAP64_NILL = NULL;

void Bitmap64RdbSave(RedisModuleIO* rdb, void* value) {
  Bitmap64* bitmap = value;
  size_t serialized_max_size = roaring64_bitmap_portable_size_in_bytes(bitmap);
  char* serialized_bitmap = rm_malloc(serialized_max_size);
  size_t serialized_size = roaring64_bitmap_portable_serialize(bitmap, serialized_bitmap);
  RedisModule_SaveStringBuffer(rdb, serialized_bitmap, serialized_size);
  rm_free(serialized_bitmap);
}

void* Bitmap64RdbLoad(RedisModuleIO* rdb, int encver) {
  if (encver != BITMAP64_ENCODING_VERSION) {
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

  uint64_t offset;
  ParseUint64OrReturn(ctx, argv[2], "offset", offset);

  bool value;
  ParseBoolOrReturn(ctx, argv[3], "value", value);

  /* Create an empty value object if the key is currently empty. */
  Bitmap64* bitmap;
  if (type == REDISMODULE_KEYTYPE_EMPTY) {
    bitmap = bitmap64_alloc();
    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
  } else {
    bitmap = RedisModule_ModuleTypeGetValue(key);
  }

  /* Set bit with value */
  bool old_value = bitmap64_setbit(bitmap, (uint64_t) offset, (char) value);

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

  uint64_t offset;
  ParseUint64OrReturn(ctx, argv[2], "offset", offset);

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

  for (size_t i = 0; i < n_offsets; i++) {
    if (!StrToUInt64(argv[2 + i], &offsets[i])) {
      rm_free(offsets);
      return RedisModule_ReplyWithError(ctx, ERRORMSG_WRONGARG_UINT64("offset"));
    }
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
 * R64.CLEARBITS <key> offset [offset1 offset2 ... offsetN] [COUNT]
 * */
int R64ClearBitsCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 3) {
    return RedisModule_WrongArity(ctx);
  }
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);

  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    return RedisModule_ReplyWithNull(ctx);
  }

  if (RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap64* bitmap = RedisModule_ModuleTypeGetValue(key);

  size_t n_offsets = (size_t) (argc - 2);
  bool count_mode = false;

  const char* last_arg = RedisModule_StringPtrLen(argv[argc - 1], NULL);

  if (strcmp(last_arg, "COUNT") == 0) {
    count_mode = true;
    n_offsets--;
  }

  uint64_t* offsets = rm_malloc(sizeof(*offsets) * n_offsets);

  for (size_t i = 0; i < n_offsets; i++) {
    if (!StrToUInt64(argv[2 + i], &offsets[i])) {
      rm_free(offsets);
      return RedisModule_ReplyWithError(ctx, ERRORMSG_WRONGARG_UINT64("offset"));
    }
  }

  RedisModule_ReplicateVerbatim(ctx);

  if (count_mode) {
    size_t count = bitmap64_clearbits_count(bitmap, n_offsets, offsets);
    RedisModule_ReplyWithLongLong(ctx, (long long) count);
  } else {
    bitmap64_clearbits(bitmap, n_offsets, offsets);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
  }

  rm_free(offsets);

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
    if (!StrToUInt64(argv[2 + i], &values[i])) {
      rm_free(values);
      return RedisModule_ReplyWithError(ctx, ERRORMSG_WRONGARG_UINT64("value"));
    }
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

  uint64_t start;
  ParseUint64OrReturn(ctx, argv[2], "start", start);

  uint64_t end;
  ParseUint64OrReturn(ctx, argv[3], "end", end);

  if (start > end) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  // Check for potential overflow in range calculation
  uint64_t range_size = (end - start) + 1;

  if (range_size > BITMAP64_MAX_RANGE_SIZE) {
    return RedisModule_ReplyWithErrorFormat(ctx, "ERR range too large: maximum %llu elements", BITMAP64_MAX_RANGE_SIZE);
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
    if (!StrToUInt64(argv[2 + i], &values[i])) {
      rm_free(values);
      return RedisModule_ReplyWithError(ctx, ERRORMSG_WRONGARG_UINT64("value"));
    }
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
    if (!StrToUInt64(argv[2 + i], &values[i])) {
      rm_free(values);
      return RedisModule_ReplyWithError(ctx, ERRORMSG_WRONGARG_UINT64("value"));
    }
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
  uint64_t start_num;
  ParseUint64OrReturn(ctx, argv[2], "start", start_num);

  uint64_t end_num;
  ParseUint64OrReturn(ctx, argv[3], "end", end_num);

  if (end_num < start_num) {
    return RedisModule_ReplyWithError(ctx, ERRORMSG_WRONGARG("end", "must >= start"));
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
  bool was_modified = bitmap64_optimize(bitmap, shrink_to_fit);

  if (was_modified) {
    RedisModule_ReplicateVerbatim(ctx);
  }

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

  uint64_t last = 0;

  if (argc == 5) {
    ParseUint64OrReturn(ctx, argv[4], "last", last);
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

  // Validate argument count (need at least: cmd, op, destkey, srckey1, srckey2)
  if (argc < 5) {
    return RedisModule_WrongArity(ctx);
  }

  uint32_t num_sources = ((uint32_t) (argc - 2));

  // Allocate arrays
  Bitmap64** bitmaps = rm_malloc(num_sources * sizeof(*bitmaps));
  RedisModuleKey** srckeys = rm_malloc(num_sources * sizeof(RedisModuleKey*));

  // Open destination key for read/write
  bool dest_allocated = false;
  Bitmap64* dest_copy = BITMAP64_NILL;

  srckeys[0] = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ | REDISMODULE_WRITE);

  if (RedisModule_KeyType(srckeys[0]) == REDISMODULE_KEYTYPE_EMPTY
    || RedisModule_ModuleTypeGetType(srckeys[0]) != Bitmap64Type) {
    bitmaps[0] = bitmap64_alloc();
    dest_allocated = true;
  } else {
    bitmaps[0] = RedisModule_ModuleTypeGetValue(srckeys[0]);
  }

  // Open and validate source keys
  for (uint32_t i = 1; i < num_sources; i++) {
    // Check if source is same as destination
    if (RedisModule_StringCompare(argv[2], argv[2 + i]) == 0) {
      if (dest_copy == BITMAP64_NILL) {
        dest_copy = roaring64_bitmap_copy(bitmaps[0]);
      }

      srckeys[i] = srckeys[0];
      bitmaps[i] = dest_copy;
      continue;
    }

    // Open source key
    srckeys[i] = RedisModule_OpenKey(ctx, argv[2 + i], REDISMODULE_READ);

    if (RedisModule_KeyType(srckeys[i]) == REDISMODULE_KEYTYPE_EMPTY) {
      bitmaps[i] = BITMAP64_NILL;
    } else if (RedisModule_ModuleTypeGetType(srckeys[i]) != Bitmap64Type) {
      if (dest_allocated) {
        bitmap64_free(bitmaps[0]);
      }

      if (dest_copy != BITMAP64_NILL) {
        bitmap64_free(dest_copy);
      }

      rm_free(bitmaps);
      rm_free(srckeys);
      return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    } else {
      bitmaps[i] = RedisModule_ModuleTypeGetValue(srckeys[i]);
    }
  }

  // Perform the bitmap operation
  operation(bitmaps[0], num_sources - 1, (const Bitmap64**) (bitmaps + 1));

  // Update destination key
  if (dest_allocated) {
    RedisModule_ModuleTypeSetValue(srckeys[0], Bitmap64Type, bitmaps[0]);
  } else {
    RedisModule_SignalModifiedKey(ctx, argv[2]);
  }

  RedisModule_ReplicateVerbatim(ctx);

  // Reply with cardinality
  uint64_t cardinality = bitmap64_get_cardinality(bitmaps[0]);
  ReplyWithUint64(ctx, cardinality);

  // Cleanup
  if (dest_copy != BITMAP64_NILL) {
    bitmap64_free(dest_copy);
  }

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
  if (RedisModule_IsKeysPositionRequest(ctx) > 0) {
    if (argc > 4) {
      for (int i = 2; i < argc; i++) {
        RedisModule_KeyAtPos(ctx, i);
      }
    }
    return REDISMODULE_OK;
  }

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
  } else if (strcmp(operation, "DIFF") == 0) {
    return R64BitOp(ctx, argv, argc, bitmap64_andnot);
  } else if (strcmp(operation, "DIFF1") == 0) {
    return R64BitOp(ctx, argv, argc, bitmap64_ornot);
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

  bool bit;
  ParseBoolOrReturn(ctx, argv[2], "bit", bit);

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

/**
 * R64.CONTAINS <key1> <key2> [ALL, ALL_STRICT]
 * */
int R64ContainsCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc < 3) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);

  RedisModuleKey* key1 = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  if (RedisModule_KeyType(key1) == REDISMODULE_KEYTYPE_EMPTY || RedisModule_ModuleTypeGetType(key1) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  RedisModuleKey* key2 = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ);
  if (RedisModule_KeyType(key2) == REDISMODULE_KEYTYPE_EMPTY || RedisModule_ModuleTypeGetType(key1) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  uint32_t mode = BITMAP_INTERSECT_MODE_NONE;

  if (argc == 4) {
    const char* mode_arg = RedisModule_StringPtrLen(argv[3], NULL);

    if (strcmp(mode_arg, "ALL") == 0) {
      mode = BITMAP_INTERSECT_MODE_ALL;
    } else if (strcmp(mode_arg, "ALL_STRICT") == 0) {
      mode = BITMAP_INTERSECT_MODE_ALL_STRICT;
    } else if (strcmp(mode_arg, "EQ") == 0) {
      mode = BITMAP_INTERSECT_MODE_EQ;
    } else {
      return RedisModule_ReplyWithErrorFormat(ctx, "ERR invalid mode argument: %s", mode_arg);
    }
  }

  Bitmap64* b1 = RedisModule_ModuleTypeGetValue(key1);
  Bitmap64* b2 = RedisModule_ModuleTypeGetValue(key2);

  RedisModule_ReplicateVerbatim(ctx);

  if (bitmap64_intersect(b1, b2, mode)) {
    RedisModule_ReplyWithLongLong(ctx, 1);
  } else {
    RedisModule_ReplyWithLongLong(ctx, 0);
  }

  return REDISMODULE_OK;
}

/**
 * R64.JACCARD <key1> <key2>
 * */
int R64JaccardCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);

  RedisModuleKey* key1 = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  if (RedisModule_KeyType(key1) == REDISMODULE_KEYTYPE_EMPTY || RedisModule_ModuleTypeGetType(key1) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  RedisModuleKey* key2 = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ);
  if (RedisModule_KeyType(key2) == REDISMODULE_KEYTYPE_EMPTY || RedisModule_ModuleTypeGetType(key1) != Bitmap64Type) {
    return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
  }

  Bitmap64* b1 = RedisModule_ModuleTypeGetValue(key1);
  Bitmap64* b2 = RedisModule_ModuleTypeGetValue(key2);

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithDouble(ctx, bitmap64_jaccard(b1, b2));

  return REDISMODULE_OK;
}

void R64Module_onShutdown(RedisModuleCtx* ctx, RedisModuleEvent e, uint64_t sub, void* data) {
  bitmap64_free(BITMAP64_NILL);
}

int R64Module_onLoad(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  BITMAP64_NILL = bitmap64_alloc();

  RedisModuleTypeMethods tm = {
      .version = REDISMODULE_TYPE_METHOD_VERSION,
      .rdb_load = Bitmap64RdbLoad,
      .rdb_save = Bitmap64RdbSave,
      .aof_rewrite = Bitmap64AofRewrite,
      .mem_usage = Bitmap64MemUsage,
      .free = Bitmap64Free
  };

  Bitmap64Type = RedisModule_CreateDataType(ctx, "roaring64", BITMAP64_ENCODING_VERSION, &tm);

  if (Bitmap64Type == NULL) {
    RedisModule_Log(ctx, "warning", "Failed to register the Bitmap64Type data type");
    return REDISMODULE_ERR;
  }

  // Register R64.* commands
#define RegisterCommand(ctx, name, cmd, mode, acl)                                                 \
  RegisterCommandWithModesAndAcls(ctx, name, cmd, mode, acl " roaring64");

  RegisterAclCategory(ctx, "roaring64");
  RegisterCommand(ctx, "R64.SETBIT", R64SetBitCommand, "write", "write");
  RegisterCommand(ctx, "R64.GETBIT", R64GetBitCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.GETBITS", R64GetBitManyCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.SETINTARRAY", R64SetIntArrayCommand, "write", "write");
  RegisterCommand(ctx, "R64.GETINTARRAY", R64GetIntArrayCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.RANGEINTARRAY", R64RangeIntArrayCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.APPENDINTARRAY", R64AppendIntArrayCommand, "write", "write");
  RegisterCommand(ctx, "R64.DELETEINTARRAY", R64DeleteIntArrayCommand, "write", "write");
  RegisterCommand(ctx, "R64.DIFF", R64DiffCommand, "write", "write");
  RegisterCommand(ctx, "R64.SETFULL", R64SetFullCommand, "write", "write");
  RegisterCommand(ctx, "R64.SETRANGE", R64SetRangeCommand, "write", "write");
  RegisterCommand(ctx, "R64.OPTIMIZE", R64OptimizeBitCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.SETBITARRAY", R64SetBitArrayCommand, "write", "write");
  RegisterCommand(ctx, "R64.GETBITARRAY", R64GetBitArrayCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.BITOP", R64BitOpCommand, "write getkeys-api", "write");
  RegisterCommand(ctx, "R64.BITCOUNT", R64BitCountCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.BITPOS", R64BitPosCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.MIN", R64MinCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.MAX", R64MaxCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.CLEAR", R64ClearCommand, "write", "write");
  RegisterCommand(ctx, "R64.CONTAINS", R64ContainsCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.JACCARD", R64JaccardCommand, "readonly", "read");
  RegisterCommand(ctx, "R64.CLEARBITS", R64ClearBitsCommand, "write", "write");

  if (RegisterR64CommandInfos(ctx) != REDISMODULE_OK) {
    RedisModule_Log(ctx, "warning", "Failed to register the R64.* commands info");
    return REDISMODULE_ERR;
  }
#undef RegisterCommand

  return REDISMODULE_OK;
}
