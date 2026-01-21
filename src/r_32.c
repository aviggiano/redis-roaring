#include "r_32.h"
#include "rmalloc.h"
#include "roaring.h"
#include "common.h"
#include "parse.h"
#include "cmd_info/command_info.h"

RedisModuleType* BitmapType = NULL;
Bitmap* BITMAP_NILL = NULL;

#define ERRORMSG_KEY_MISSED "Roaring: key does not exist"
#define ERRORMSG_KEY_EXISTS "Roaring: key already exist"
#define ERRORMSG_SET_VALUE "Roaring: error setting value"
#define ERRORMSG_RANGE_LIMIT "Roaring: range too large: maximum %llu elements"

#define INNER_ERROR(x) \
  do { \
    RedisModule_ReplyWithError(ctx, x); \
    return REDISMODULE_ERR; \
  } while(0)

static int GetBitmapKey(RedisModuleCtx* ctx, RedisModuleString* keyName, Bitmap** value_out, int mode) {
  RedisModuleKey* key = RedisModule_OpenKey(ctx, keyName, mode);
  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    RedisModule_CloseKey(key);
    INNER_ERROR(ERRORMSG_KEY_MISSED);
  } else if (RedisModule_ModuleTypeGetType(key) != BitmapType) {
    RedisModule_CloseKey(key);
    INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
  }
  *value_out = RedisModule_ModuleTypeGetValue(key);
  RedisModule_CloseKey(key);
  return REDISMODULE_OK;
}

static int TryGetBitmapKey(RedisModuleCtx* ctx, RedisModuleString* keyName, Bitmap** value_out, RedisModuleKey** key_out, int mode) {
  RedisModuleKey* key = RedisModule_OpenKey(ctx, keyName, mode);
  if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
    *key_out = key;
    *value_out = BITMAP_NILL;
  } else if (RedisModule_ModuleTypeGetType(key) != BitmapType) {
    RedisModule_CloseKey(key);
    INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
  } else {
    *key_out = key;
    *value_out = RedisModule_ModuleTypeGetValue(key);
  }

  return REDISMODULE_OK;
}

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

/**
 * R.SETFULL <key>
 * */
int RSetFullCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap != BITMAP_NILL) {
    INNER_ERROR(ERRORMSG_KEY_EXISTS);
  }

  bitmap = bitmap_from_range(0, UINT32_MAX);
  //NOTE bitmap from range is an right open interval, to set full bit for key, we need do it manually
  bitmap_setbit(bitmap, UINT32_MAX, 1);
  RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R.SETRANGE <key> <start_num> <end_num>
 * */
int RSetRangeCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  uint32_t start_num;
  ParseUint32OrReturn(ctx, argv[2], "start", start_num);

  uint32_t end_num;
  ParseUint32OrReturn(ctx, argv[3], "end", end_num);

  if (end_num < start_num) {
    INNER_ERROR(ERRORMSG_WRONGARG("end", "must be >= start"));
  }

  if (bitmap == BITMAP_NILL) {
    bitmap = bitmap_from_range(start_num, end_num);
    RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
  } else {
    roaring_bitmap_add_range(bitmap, start_num, end_num);
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
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  uint32_t offset;
  ParseUint32OrReturn(ctx, argv[2], "offset", offset);

  bool value;
  ParseBoolOrReturn(ctx, argv[3], "value", value);

  /* Create an empty value object if the key is currently empty. */
  if (bitmap == BITMAP_NILL) {
    uint32_t values[] = { offset };
    bitmap = bitmap_from_int_array(1, values);
    RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithLongLong(ctx, 0);
  }

  /* Set bit with value */
  bool old_value = bitmap_setbit(bitmap, offset, value);
  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithLongLong(ctx, old_value);
}

/**
 * R.GETBIT <key> <offset>
 * */
int RGetBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }

  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP_NILL) {
    return RedisModule_ReplyWithLongLong(ctx, 0);
  }

  uint32_t offset;
  ParseUint32OrReturn(ctx, argv[2], "offset", offset);

  char value = bitmap_getbit(bitmap, offset);

  return RedisModule_ReplyWithLongLong(ctx, value);
}

/**
 * R.GETBITS <key> offset [offset1 offset2 ... offsetN]
 * */
int RGetBitManyCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP_NILL) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  size_t n_offsets = (size_t) (argc - 2);
  uint32_t* offsets = rm_malloc(sizeof(*offsets) * n_offsets);

  for (int i = 0; i < n_offsets; i++) {
    if (!StrToUInt32(argv[2 + i], &offsets[i])) {
      rm_free(offsets);
      INNER_ERROR(ERRORMSG_WRONGARG_UINT32("offset"));
    }
  }

  bool* results = bitmap_getbits(bitmap, n_offsets, offsets);

  RedisModule_ReplyWithArray(ctx, n_offsets);

  for (size_t i = 0; i < n_offsets; i++) {
    if (RedisModule_ReplyWithLongLong(ctx, (long long) results[i]) == REDISMODULE_ERR) {
      rm_free(offsets);
      rm_free(results);
      return REDISMODULE_ERR;
    }
  }

  rm_free(offsets);
  rm_free(results);

  return REDISMODULE_OK;
}

/**
 * R.CLEARBITS <key> offset [offset1 offset2 ... offsetN] [COUNT]
 * */
int RClearBitsCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP_NILL) {
    return RedisModule_ReplyWithNull(ctx);
  }

  size_t n_offsets = (size_t) (argc - 2);
  bool count_mode = false;

  const char* last_arg = RedisModule_StringPtrLen(argv[argc - 1], NULL);

  if (strcmp(last_arg, "COUNT") == 0) {
    count_mode = true;
    n_offsets--;
  }

  uint32_t* offsets = rm_malloc(sizeof(*offsets) * n_offsets);

  for (int i = 0; i < n_offsets; i++) {
    if (!StrToUInt32(argv[2 + i], &offsets[i])) {
      rm_free(offsets);
      INNER_ERROR(ERRORMSG_WRONGARG_UINT32("offset"));
    }
  }

  RedisModule_ReplicateVerbatim(ctx);

  if (count_mode) {
    size_t count = bitmap_clearbits_count(bitmap, n_offsets, offsets);
    rm_free(offsets);
    return RedisModule_ReplyWithLongLong(ctx, (long long) count);
  }

  bitmap_clearbits(bitmap, n_offsets, offsets);
  rm_free(offsets);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R.OPTIMIZE <key> [MEM]
 * */
int ROptimizeBitCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  Bitmap* bitmap;

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

  bool was_modified = bitmap_optimize(bitmap, shrink_to_fit);

  if (was_modified) {
    RedisModule_ReplicateVerbatim(ctx);
  }

  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}


/**
 * R.SETINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int RSetIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  size_t length = (size_t) (argc - 2);
  uint32_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    if (!StrToUInt32(argv[2 + i], &values[i])) {
      rm_free(values);
      INNER_ERROR(ERRORMSG_WRONGARG_UINT32("value"));
    }
  }

  bitmap = bitmap_from_int_array(length, values);
  rm_free(values);

  if (RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap) != REDISMODULE_OK) {
    RedisModule_CloseKey(key);
    INNER_ERROR(ERRORMSG_SET_VALUE);
  }

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R.DIFF <dest> <decreasing> <deductible>
 * */
int RDiffCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  // open destkey for writing
  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  Bitmap* decreasing_bitmap;
  Bitmap* deductible_bitmap;

  if (GetBitmapKey(ctx, argv[2], &decreasing_bitmap, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (GetBitmapKey(ctx, argv[3], &deductible_bitmap, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  Bitmap* result = roaring_bitmap_andnot(decreasing_bitmap, deductible_bitmap);

  if (RedisModule_ModuleTypeSetValue(key, BitmapType, result) != REDISMODULE_OK) {
    RedisModule_CloseKey(key);
    INNER_ERROR(ERRORMSG_SET_VALUE);
  }

  RedisModule_CloseKey(key);
  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}


/**
 * R.APPENDINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int RAppendIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  size_t length = (size_t) (argc - 2);
  uint32_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    if (!StrToUInt32(argv[2 + i], &values[i])) {
      rm_free(values);
      INNER_ERROR(ERRORMSG_WRONGARG_UINT32("value"));
    }
  }

  if (bitmap == BITMAP_NILL) {
    bitmap = bitmap_from_int_array(length, values);
    rm_free(values);

    if (RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap) != REDISMODULE_OK) {
      RedisModule_CloseKey(key);
      INNER_ERROR(ERRORMSG_SET_VALUE);
    }
  } else {
    roaring_bitmap_add_many(bitmap, length, values);
    rm_free(values);
    RedisModule_CloseKey(key);
  }

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R.DELETEINTARRAY <key> <value1> [<value2> <value3> ... <valueN>]
 * */
int RDeleteIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP_NILL) {
    bitmap = bitmap_alloc();

    RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap);
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithSimpleString(ctx, "OK");
  }

  size_t length = (size_t) (argc - 2);
  uint32_t* values = rm_malloc(sizeof(*values) * length);
  for (int i = 0; i < length; i++) {
    if (!StrToUInt32(argv[2 + i], &values[i])) {
      rm_free(values);
      INNER_ERROR(ERRORMSG_WRONGARG_UINT32("value"));
    }
  }

  roaring_bitmap_remove_many(bitmap, length, values);
  rm_free(values);

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R.RANGEINTARRAY <key> <start> <end>
 * */
int RRangeIntArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  uint32_t start;
  ParseUint32OrReturn(ctx, argv[2], "start", start);

  uint32_t end;
  ParseUint32OrReturn(ctx, argv[3], "end", end);

  if (start > end) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  // Check for potential overflow in range calculation
  uint32_t range_size = (end - start) + 1;

  if (range_size > BITMAP_MAX_RANGE_SIZE) {
    return RedisModule_ReplyWithErrorFormat(ctx, ERRORMSG_RANGE_LIMIT, BITMAP_MAX_RANGE_SIZE);
  }

  if (bitmap == BITMAP_NILL) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  size_t count;
  uint32_t* array = bitmap_range_int_array(bitmap, start, end, &count);

  if (array == NULL) {
    INNER_ERROR("ERR out of memory");
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
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP_NILL) {
    return RedisModule_ReplyWithEmptyArray(ctx);
  }

  size_t n = 0;
  uint32_t* array = bitmap_get_int_array(bitmap, &n);

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
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  size_t len;
  const char* array = RedisModule_StringPtrLen(argv[2], &len);
  bitmap = bitmap_from_bit_array(len, array);

  if (RedisModule_ModuleTypeSetValue(key, BitmapType, bitmap) != REDISMODULE_OK) {
    RedisModule_CloseKey(key);
    INNER_ERROR(ERRORMSG_SET_VALUE);
  }

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

/**
 * R.GETBITARRAY <key>
 * */
int RGetBitArrayCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP_NILL) {
    return RedisModule_ReplyWithSimpleString(ctx, "");
  }

  size_t size;
  char* array = bitmap_get_bit_array(bitmap, &size);
  RedisModule_ReplyWithStringBuffer(ctx, array, size);

  bitmap_free_bit_array(array);

  return REDISMODULE_OK;
}

/**
 * R.BITOP <op> <key> <keys...>
 * */
int RBitOp(RedisModuleCtx* ctx, RedisModuleString** argv, int argc, void (*operation)(Bitmap*, uint32_t, const Bitmap**)) {
  if (argc < 5) {
    return RedisModule_WrongArity(ctx);
  }

  uint32_t num_sources = ((uint32_t) (argc - 2));

  // Allocate arrays
  Bitmap** bitmaps = rm_malloc(num_sources * sizeof(*bitmaps));
  RedisModuleKey** srckeys = rm_malloc(num_sources * sizeof(RedisModuleKey*));

  // Open destination key for read/write
  bool dest_allocated = false;
  Bitmap* dest_copy = BITMAP_NILL;

  if (TryGetBitmapKey(ctx, argv[2], &bitmaps[0], &srckeys[0], REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmaps[0] == BITMAP_NILL) {
    bitmaps[0] = bitmap_alloc();
    dest_allocated = true;
  }

  // Open and validate source keys
  for (uint32_t i = 1; i < num_sources; i++) {
    // Check if source is same as destination
    if (RedisModule_StringCompare(argv[2], argv[2 + i]) == 0) {
      if (dest_copy == BITMAP_NILL) {
        dest_copy = roaring_bitmap_copy(bitmaps[0]);
      }

      srckeys[i] = srckeys[0];
      bitmaps[i] = dest_copy;
      continue;
    }

    // Open source key
    if (TryGetBitmapKey(ctx, argv[2 + i], &bitmaps[i], &srckeys[i], REDISMODULE_READ) == REDISMODULE_ERR) {
      if (dest_allocated) {
        bitmap_free(bitmaps[0]);
      }

      rm_free(bitmaps);
      rm_free(srckeys);

      if (dest_copy != BITMAP_NILL) {
        bitmap_free(dest_copy);
      }

      INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
    }
  }

  // Perform the bitmap operation
  operation(bitmaps[0], num_sources - 1, (const Bitmap**) (bitmaps + 1));

  // Update destination key
  if (dest_allocated) {
    RedisModule_ModuleTypeSetValue(srckeys[0], BitmapType, bitmaps[0]);
  }

  RedisModule_ReplicateVerbatim(ctx);

  // Reply with cardinality
  uint64_t cardinality = bitmap_get_cardinality(bitmaps[0]);
  ReplyWithUint64(ctx, cardinality);

  // Cleanup
  if (dest_copy != BITMAP_NILL) {
    bitmap_free(dest_copy);
  }

  rm_free(bitmaps);
  rm_free(srckeys);

  return REDISMODULE_OK;
}

int RBitFlip(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  long long last = -1;
  if (argc == 5) {
    uint32_t last_pared;
    ParseUint32OrReturn(ctx, argv[4], "last", last_pared);
    last = (long long) last_pared;
  } else if (argc > 5) {
    return RedisModule_WrongArity(ctx);
  }

  // open destkey for writing
  RedisModuleKey* destkey;
  Bitmap* destbitmap;

  if (TryGetBitmapKey(ctx, argv[2], &destbitmap, &destkey, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  // checks for srckey types
  RedisModuleKey* srckey;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[3], &bitmap, &destkey, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
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
  RedisModule_ReplicateVerbatim(ctx);

  // Integer reply: The size of the string stored in the destination key
  // (adapted to cardinality)
  uint64_t cardinality = bitmap_get_cardinality(result);
  return ReplyWithUint64(ctx, cardinality);
}

/**
 * BITOP AND destkey srckey1 srckey2 srckey3 ... srckeyN
 * BITOP OR destkey srckey1 srckey2 srckey3 ... srckeyN
 * BITOP XOR destkey srckey1 srckey2 srckey3 ... srckeyN
 * BITOP NOT destkey srckey
 * R.BITOP <operation> <destkey> <key> [<key> ...]
 * */
int RBitOpCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
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
  } else if (strcmp(operation, "DIFF") == 0) {
    return RBitOp(ctx, argv, argc, bitmap_andnot);
  } else if (strcmp(operation, "DIFF1") == 0) {
    return RBitOp(ctx, argv, argc, bitmap_ornot);
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
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP_NILL) {
    return ReplyWithUint64(ctx, 0);
  }

  uint64_t count = bitmap_get_cardinality(bitmap);

  return ReplyWithUint64(ctx, count);
}


/**
 * R.BITPOS <key> <bit>
 * */
int RBitPosCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  bool bit;
  ParseBoolOrReturn(ctx, argv[2], "bit", bit);

  int64_t pos = 1;
  if (bitmap != BITMAP_NILL) {
    if (bit) {
      pos = bitmap_get_nth_element_present(bitmap, 1);
    } else {
      pos = bitmap_get_nth_element_not_present(bitmap, 1);
    }
  }

  return RedisModule_ReplyWithLongLong(ctx, (long long) pos);
}

/**
 * R.MIN <key>
 * */
int RMinCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  long long result = -1;

  if (bitmap != BITMAP_NILL && !bitmap_is_empty(bitmap)) {
    result = bitmap_min(bitmap);
  }

  return RedisModule_ReplyWithLongLong(ctx, result);
}

/**
 * R.MAX <key>
 * */
int RMaxCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  long long result = -1;
  if (bitmap != BITMAP_NILL && !bitmap_is_empty(bitmap)) {
    result = bitmap_max(bitmap);
  }

  RedisModule_ReplyWithLongLong(ctx, result);
  return REDISMODULE_OK;
}

/**
 * R.CLEAR <key>
 * */
int RClearCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  RedisModuleKey* key;
  Bitmap* bitmap;

  if (TryGetBitmapKey(ctx, argv[1], &bitmap, &key, REDISMODULE_READ | REDISMODULE_WRITE) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (bitmap == BITMAP_NILL) {
    return RedisModule_ReplyWithNull(ctx);
  }

  uint64_t count = bitmap_get_cardinality(bitmap);

  if (count > 0) {
    roaring_bitmap_clear(bitmap);
  }

  RedisModule_ReplicateVerbatim(ctx);
  return RedisModule_ReplyWithLongLong(ctx, (long long) count);
}

/**
 * R.CONTAINS <key1> <key2> [ALL, ALL_STRICT]
 * */
int RContainsCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  Bitmap* b1;
  Bitmap* b2;

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

  if (bitmap_intersect(b1, b2, mode)) {
    return RedisModule_ReplyWithLongLong(ctx, 1);
  } else {
    return RedisModule_ReplyWithLongLong(ctx, 0);
  }
}

/**
 * R.JACCARD <key1> <key2>
 * */
int RJaccardCommand(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  RedisModule_AutoMemory(ctx);
  Bitmap* b1;
  Bitmap* b2;

  if (GetBitmapKey(ctx, argv[1], &b1, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (GetBitmapKey(ctx, argv[2], &b2, REDISMODULE_READ) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return RedisModule_ReplyWithDouble(ctx, bitmap_jaccard(b1, b2));
}

void R32Module_onShutdown(RedisModuleCtx* ctx, RedisModuleEvent e, uint64_t sub, void* data) {
  bitmap_free(BITMAP_NILL);
}

int R32Module_onLoad(RedisModuleCtx* ctx, RedisModuleString** argv, int argc) {
  BITMAP_NILL = bitmap_alloc();

  RedisModuleTypeMethods tm = {
      .version = REDISMODULE_TYPE_METHOD_VERSION,
      .rdb_load = BitmapRdbLoad,
      .rdb_save = BitmapRdbSave,
      .aof_rewrite = BitmapAofRewrite,
      .mem_usage = BitmapMemUsage,
      .free = BitmapFree
  };

  BitmapType = RedisModule_CreateDataType(ctx, "reroaring", BITMAP_ENCODING_VERSION, &tm);

  if (BitmapType == NULL) {
    RedisModule_Log(ctx, "warning", "Failed to register the BitmapType data type");
    return REDISMODULE_ERR;
  }

  // Register R.* commands
#define RegisterCommand(ctx, name, cmd, mode, acl)                                                 \
  RegisterCommandWithModesAndAcls(ctx, name, cmd, mode, acl " roaring");

  RegisterAclCategory(ctx, "roaring");

  RegisterCommand(ctx, "R.SETBIT", RSetBitCommand, "write", "write");
  RegisterCommand(ctx, "R.GETBIT", RGetBitCommand, "readonly", "read");
  RegisterCommand(ctx, "R.GETBITS", RGetBitManyCommand, "readonly", "read");
  RegisterCommand(ctx, "R.CLEARBITS", RClearBitsCommand, "write", "write");
  RegisterCommand(ctx, "R.SETINTARRAY", RSetIntArrayCommand, "write", "write");
  RegisterCommand(ctx, "R.GETINTARRAY", RGetIntArrayCommand, "readonly", "read");
  RegisterCommand(ctx, "R.RANGEINTARRAY", RRangeIntArrayCommand, "readonly", "read");
  RegisterCommand(ctx, "R.APPENDINTARRAY", RAppendIntArrayCommand, "write", "write");
  RegisterCommand(ctx, "R.DELETEINTARRAY", RDeleteIntArrayCommand, "write", "write");
  RegisterCommand(ctx, "R.DIFF", RDiffCommand, "write", "write");
  RegisterCommand(ctx, "R.SETFULL", RSetFullCommand, "write", "write");
  RegisterCommand(ctx, "R.SETRANGE", RSetRangeCommand, "write", "write");
  RegisterCommand(ctx, "R.OPTIMIZE", ROptimizeBitCommand, "readonly", "read");
  RegisterCommand(ctx, "R.SETBITARRAY", RSetBitArrayCommand, "write", "write");
  RegisterCommand(ctx, "R.GETBITARRAY", RGetBitArrayCommand, "readonly", "read");
  RegisterCommand(ctx, "R.BITOP", RBitOpCommand, "write getkeys-api", "write");
  RegisterCommand(ctx, "R.BITCOUNT", RBitCountCommand, "readonly", "read");
  RegisterCommand(ctx, "R.BITPOS", RBitPosCommand, "readonly", "read");
  RegisterCommand(ctx, "R.MIN", RMinCommand, "readonly", "read");
  RegisterCommand(ctx, "R.MAX", RMaxCommand, "readonly", "read");
  RegisterCommand(ctx, "R.CLEAR", RClearCommand, "write", "write");
  RegisterCommand(ctx, "R.CONTAINS", RContainsCommand, "readonly", "read");
  RegisterCommand(ctx, "R.JACCARD", RJaccardCommand, "readonly", "read");

  if (RegisterRCommandInfos(ctx) != REDISMODULE_OK) {
    RedisModule_Log(ctx, "warning", "Failed to register the R.* commands info");
    return REDISMODULE_ERR;
  }
#undef RegisterCommand

  return REDISMODULE_OK;
}
