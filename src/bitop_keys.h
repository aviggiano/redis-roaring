#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "redismodule.h"

typedef struct {
  int pos;
  int flags;
} BitOpKeyPosition;

typedef void (*BitOpKeyReporter)(void* ctx, int pos, int flags);

static inline bool BitOpIsVariadicOperation(const char* operation) {
  return strcmp(operation, "AND") == 0
      || strcmp(operation, "OR") == 0
      || strcmp(operation, "XOR") == 0
      || strcmp(operation, "ANDOR") == 0
      || strcmp(operation, "ONE") == 0
      || strcmp(operation, "DIFF") == 0
      || strcmp(operation, "DIFF1") == 0;
}

static inline void BitOpReportRedisKey(void* ctx, int pos, int flags) {
  RedisModuleCtx* rm_ctx = ctx;
  if (RMAPI_FUNC_SUPPORTED(RedisModule_KeyAtPosWithFlags)) {
    RedisModule_KeyAtPosWithFlags(rm_ctx, pos, flags);
  } else {
    RedisModule_KeyAtPos(rm_ctx, pos);
  }
}

static inline size_t BitOpForEachKeyPosition(const char* operation, int argc, void* ctx, BitOpKeyReporter reporter) {
  if (operation == NULL) {
    return 0;
  }

  if (strcmp(operation, "NOT") == 0) {
    if (argc < 4 || argc > 5) {
      return 0;
    }

    if (reporter != NULL) {
      reporter(ctx, 2, REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_INSERT);
      reporter(ctx, 3, REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS);
    }

    return 2;
  }

  if (!BitOpIsVariadicOperation(operation) || argc < 5) {
    return 0;
  }

  if (reporter != NULL) {
    reporter(ctx, 2, REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_INSERT);
  }

  size_t count = 1;
  for (int pos = 3; pos < argc; pos++) {
    if (reporter != NULL) {
      reporter(ctx, pos, REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS);
    }
    count++;
  }

  return count;
}
