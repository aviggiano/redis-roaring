#include "r_64.h"
#include "rmalloc.h"
#include "roaring.h"
#include "common.h"
#include "parse.h"
#include "cmd_info/command_info.h"

RedisModuleType* Bitmap64Type = NULL;
Bitmap64* BITMAP64_NILL = NULL;

#define ERRORMSG_KEY_MISSED "Roaring: key does not exist"
#define ERRORMSG_KEY_EXISTS "Roaring: key already exist"
#define ERRORMSG_SET_VALUE "Roaring: error setting value"
#define ERRORMSG_RANGE_LIMIT "Roaring: range too large: maximum %llu elements"

#define INNER_ERROR(x) \
  do { \
    RedisModule_ReplyWithError(ctx, x); \
    return REDISMODULE_ERR; \
  } while(0)

static int GetBitmapKey(RedisModuleCtx* ctx, RedisModuleString* keyName, Bitmap64** value_out, int mode) {
  RedisModuleKey* key = RedisModule_OpenKey(ctx, keyName, mode);
  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_CloseKey(key);
    INNER_ERROR(ERRORMSG_KEY_MISSED);
  } else if (RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    RedisModule_CloseKey(key);
    INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  *value_out = RedisModule_ModuleTypeGetValue(key);
  RedisModule_CloseKey(key);
  return REDISMODULE_OK;
}

static int TryGetBitmapKey(RedisModuleCtx* ctx, RedisModuleString* keyName, Bitmap64** value_out, RedisModuleKey** key_out, int mode) {
  RedisModuleKey* key = RedisModule_OpenKey(ctx, keyName, mode);
  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    *key_out = key;
    *value_out = BITMAP64_NILL;
  } else if (RedisModule_ModuleTypeGetType(key) != Bitmap64Type) {
    RedisModule_CloseKey(key);
    INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
  } else {
    *key_out = key;
    *value_out = RedisModule_ModuleTypeGetValue(key);
  }

  return REDISMODULE_OK;
}

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

/**
 * R64.SETBIT <key> <offset> <value>
 * */
int R64SetBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  uint64_t offset;
  ParseUint64OrReturn(ctx, argv[2], "offset", offset);

  bool value;
  ParseBoolOrReturn(ctx, argv[3], "value", value);

  /* Create an empty value object if the key is currently empty. */
  if (bitmap == BITMAP64_NILL) {
    uint64_t values[] = { offset };
    bitmap = bitmap64_from_int_array(1, values);
    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithLongLong(ctx, 0);
  }

  /* Set bit with value */
  bool old_value = bitmap64_setbit(bitmap, offset, value);
  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithLongLong(ctx, old_value);
}

/**
 * R64.GETBIT <key> <offset>
 * */
int R64GetBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  uint64_t offset;
  ParseUint64OrReturn(ctx, argv[2], "offset", offset);

  if (bitmap == BITMAP64_NILL) {
    return RedisModule_ReplyWithLongLong(ctx, 0);
  }

  char value = bitmap64_getbit(bitmap, (uint64_t) offset);

  return RedisModule_ReplyWithLongLong(ctx, value);
}

/**
 * R64.GETBITS <key> offset [offset1 offset2 ... offsetN]
 * */
int R64GetBitManyCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP64_NILL) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  size_t n_offsets = (size_t) (argc - 2);
  uint64_t* offsets = rm_malloc(sizeof(*offsets) * n_offsets);

  for (size_t i = 0; i < n_offsets; i++) {
    if (!StrToUInt64(argv[2 + i], &offsets[i])) {
      rm_free(offsets);
      INNER_ERROR(ERRORMSG_WRONGARG_UINT64("offset"));
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
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP64_NILL) {
    return RedisModule_ReplyWithNull(ctx);
  }

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
      INNER_ERROR(ERRORMSG_WRONGARG_UINT64("offset"));
    }
  }

  RedisModule_ReplicateVerbatim(ctx);

  if (count_mode) {
    size_t count = bitmap64_clearbits_count(bitmap, n_offsets, offsets);
    rm_free(offsets);
    return RedisModule_ReplyWithLongLong(ctx, (long long) count);
  }

  bitmap64_clearbits(bitmap, n_offsets, offsets);
  rm_free(offsets);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R64.SETINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int R64SetIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  size_t length = (size_t) (argc - 2);
  uint64_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    if (!StrToUInt64(argv[2 + i], &values[i])) {
      rm_free(values);
      INNER_ERROR(ERRORMSG_WRONGARG_UINT64("value"));
    }
  }

  bitmap = bitmap64_from_int_array(length, values);
  rm_free(values);

  if (RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap) != REDISMODULE_OK) {
    RedisModule_CloseKey(key);
    INNER_ERROR(ERRORMSG_SET_VALUE);
  }

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R64.GETINTARRAY <key>
 * */
int R64GetIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP64_NILL) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  uint64_t n = 0;
  uint64_t* array = bitmap64_get_int_array(bitmap, &n);

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
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
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
    return RedisModule_ReplyWithErrorFormat(ctx, ERRORMSG_RANGE_LIMIT, BITMAP64_MAX_RANGE_SIZE);
  }

  if (bitmap == BITMAP64_NILL) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  uint64_t count;
  uint64_t* array = bitmap64_range_int_array(bitmap, start, end, &count);

  if (array == NULL) {
    INNER_ERROR("ERR out of memory");
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
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  size_t length = (size_t) (argc - 2);
  uint64_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    if (!StrToUInt64(argv[2 + i], &values[i])) {
      rm_free(values);
      INNER_ERROR(ERRORMSG_WRONGARG_UINT64("value"));
    }
  }

  if (bitmap == BITMAP64_NILL) {
    bitmap = bitmap64_from_int_array(length, values);
    rm_free(values);

    if (RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap) != REDISMODULE_OK) {
      RedisModule_CloseKey(key);
      INNER_ERROR(ERRORMSG_SET_VALUE);
    }
  } else {
    roaring64_bitmap_add_many(bitmap, length, values);
    rm_free(values);
    RedisModule_CloseKey(key);
  }

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R64.DELETEINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int R64DeleteIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP64_NILL) {
    bitmap = bitmap64_alloc();

    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithSimpleString(ctx, "OK");
  }

  size_t length = (size_t) (argc - 2);
  uint64_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    if (!StrToUInt64(argv[2 + i], &values[i])) {
      rm_free(values);
      INNER_ERROR(ERRORMSG_WRONGARG_UINT64("value"));
    }
  }

  roaring64_bitmap_remove_many(bitmap, length, values);
  rm_free(values);

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R64.DIFF <dest> <decreasing> <deductible>
 * */
int R64DiffCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  // open destkey for writing
  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  Bitmap64* decreasing_bitmap;
  Bitmap64* deductible_bitmap;

  if (GetBitmapKey(ctx, argv[2], &decreasing_bitmap, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (GetBitmapKey(ctx, argv[3], &deductible_bitmap, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  Bitmap64* result = roaring64_bitmap_andnot(decreasing_bitmap, deductible_bitmap);

  if (RedisModule_ModuleTypeSetValue(key, Bitmap64Type, result) != REDISMODULE_OK) {
    RedisModule_CloseKey(key);
    INNER_ERROR(ERRORMSG_SET_VALUE);
  }

  RedisModule_CloseKey(key);
  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R64.SETFULL <key>
 * */
int R64SetFullCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap != BITMAP64_NILL) {
    INNER_ERROR(ERRORMSG_KEY_EXISTS);
  }

  bitmap = bitmap64_from_range(0, UINT64_MAX);
  //NOTE bitmap from range is an right open interval, to set full bit for key, we need do it manually
  bitmap64_setbit(bitmap, UINT64_MAX, 1);
  RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}


/**
 * R64.SETRANGE <key> <start_num> <end_num>
 * */
int R64SetRangeCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  uint64_t start_num;
  ParseUint64OrReturn(ctx, argv[2], "start", start_num);

  uint64_t end_num;
  ParseUint64OrReturn(ctx, argv[3], "end", end_num);

  if (end_num < start_num) {
    INNER_ERROR(ERRORMSG_WRONGARG("end", "must >= start"));
  }

  if (bitmap == BITMAP64_NILL) {
    bitmap = bitmap64_from_range(start_num, end_num);
    RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap);
  } else {
    roaring64_bitmap_add_range(bitmap, start_num, end_num);
  }

  RedisModule_ReplicateVerbatim(ctx);
  RedisModule_ReplyWithSimpleString(ctx, "OK");
  return REDISMODULE_OK;
}

/**
 * R64.OPTIMIZE <key> [MEM]
 * */
int R64OptimizeBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  Bitmap64* bitmap;

  if (GetBitmapKey(ctx, argv[1], &bitmap, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  int shrink_to_fit = 0;

  if (argc > 2) {
    const char* option = RedisModule_StringPtrLen(argv[2], NULL);
    if (strcmp(option, "MEM") == 0) {
      shrink_to_fit = 1;
    }
  }

  bool was_modified = bitmap64_optimize(bitmap, shrink_to_fit);

  if (was_modified) {
    RedisModule_ReplicateVerbatim(ctx);
  }

  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R64.SETBITARRAY <key> <value1>
 * */
int R64SetBitArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  size_t len;
  const char* array = RedisModule_StringPtrLen(argv[2], &len);
  bitmap = bitmap64_from_bit_array(len, array);

  if (RedisModule_ModuleTypeSetValue(key, Bitmap64Type, bitmap) != REDISMODULE_OK) {
    RedisModule_CloseKey(key);
    INNER_ERROR(ERRORMSG_SET_VALUE);
  }

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R64.GETBITARRAY <key>
 * */
int R64GetBitArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP64_NILL) {
    return RedisModule_ReplyWithSimpleString(ctx, "");
  }

  uint64_t size;
  char* array = bitmap64_get_bit_array(bitmap, &size);
  RedisModule_ReplyWithStringBuffer(ctx, array, size);

  bitmap_free_bit_array(array);

  return REDISMODULE_OK;
}

int R64BitFlip(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  uint64_t last = 0;

  if (argc == 5) {
    ParseUint64OrReturn(ctx, argv[4], "last", last);
  } else if (argc > 5) {
    return RedisModule_WrongArity(ctx);
  }

  // open destkey for writing
  RedisModuleKey* destkey;
  Bitmap64* destbitmap;

  if (TryGetBitmapKey(ctx, argv[2], &destbitmap, &destkey, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  // checks for srckey types
  RedisModuleKey* srckey;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[3], &bitmap, &destkey, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
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
  RedisModule_ReplicateVerbatim(ctx);

  // Integer reply: The size of the string stored in the destination key
  // (adapted to cardinality)
  uint64_t cardinality = bitmap64_get_cardinality(result);
  return ReplyWithUint64(ctx, cardinality);
}

int R64BitOp(RedisModuleCtx* ctx, RedisModuleString** argv, int argc, void (*operation)(Bitmap64*, uint32_t, const Bitmap64**)) {
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

  if (TryGetBitmapKey(ctx, argv[2], &bitmaps[0], &srckeys[0], REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmaps[0] == BITMAP64_NILL) {
    bitmaps[0] = bitmap64_alloc();
    dest_allocated = true;
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
    if (TryGetBitmapKey(ctx, argv[2 + i], &bitmaps[i], &srckeys[i], REDISMODULE_READ) == REDISMODULE_ERR) {
      if (dest_allocated) {
        bitmap64_free(bitmaps[0]);
      }

      rm_free(bitmaps);
      rm_free(srckeys);

      if (dest_copy != BITMAP64_NILL) {
        bitmap64_free(dest_copy);
      }

      INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
    }
  }

  // Perform the bitmap operation
  operation(bitmaps[0], num_sources - 1, (const Bitmap64**) (bitmaps + 1));

  // Update destination key
  if (dest_allocated) {
    RedisModule_ModuleTypeSetValue(srckeys[0], Bitmap64Type, bitmaps[0]);
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
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP64_NILL) {
    return ReplyWithUint64(ctx, 0);
  }

  uint64_t count = bitmap64_get_cardinality(bitmap);

  return ReplyWithUint64(ctx, count);
}

/**
 * R64.BITPOS <key> <bit>
 * */
int R64BitPosCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  bool bit;
  ParseBoolOrReturn(ctx, argv[2], "bit", bit);

  bool found = false;
  uint64_t pos;

  if (bitmap != BITMAP64_NILL) {
    if (bit == 1) {
      pos = bitmap64_get_nth_element_present(bitmap, 1, &found);
    } else {
      pos = bitmap64_get_nth_element_not_present(bitmap, 1, &found);
      found = true;
    }
  }

  if (found) {
    return ReplyWithUint64(ctx, pos);
  } else {
    return RedisModule_ReplyWithLongLong(ctx, -1);
  }
}

/**
 * R64.MIN <key>
 * */
int R64MinCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap != BITMAP64_NILL && !bitmap64_is_empty(bitmap)) {
    uint64_t result = bitmap64_min(bitmap);
    return ReplyWithUint64(ctx, result);
  }

  return RedisModule_ReplyWithLongLong(ctx, -1);
}

/**
 * R64.MAX <key>
 * */
int R64MaxCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap != BITMAP64_NILL && !bitmap64_is_empty(bitmap)) {
    uint64_t result = bitmap64_max(bitmap);
    return ReplyWithUint64(ctx, result);
  }

  return RedisModule_ReplyWithLongLong(ctx, -1);
}

/**
 * R64.CLEAR <key>
 * */
int R64ClearCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap64* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP64_NILL) {
    return RedisModule_ReplyWithNull(ctx);
  }

  uint64_t count = bitmap64_get_cardinality(bitmap);

  if (count > 0) {
    roaring64_bitmap_clear(bitmap);
  }

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithLongLong(ctx, (long long) count);
}

/**
 * R64.CONTAINS <key1> <key2> [ALL, ALL_STRICT]
 * */
int R64ContainsCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  Bitmap64* b1;
  Bitmap64* b2;

  if (GetBitmapKey(ctx, argv[1], &b1, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (GetBitmapKey(ctx, argv[2], &b2, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
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

  RedisModule_ReplicateVerbatim(ctx);

  if (bitmap64_intersect(b1, b2, mode)) {
    return RedisModule_ReplyWithLongLong(ctx, 1);
  } else {
    return RedisModule_ReplyWithLongLong(ctx, 0);
  }
}

/**
 * R64.JACCARD <key1> <key2>
 * */
int R64JaccardCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  Bitmap64* b1;
  Bitmap64* b2;

  if (GetBitmapKey(ctx, argv[1], &b1, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (GetBitmapKey(ctx, argv[2], &b2, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return RedisModule_ReplyWithDouble(ctx, bitmap64_jaccard(b1, b2));
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
