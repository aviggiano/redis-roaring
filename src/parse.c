#include "parse.h"
#include <limits.h>

static inline size_t uint64_to_string(uint64_t value, char* buffer) {
  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return 1;
  }

  char* ptr = buffer;
  char* start = buffer;

  // Extract digits in reverse order
  while (value > 0) {
    *ptr++ = '0' + (value % 10);
    value /= 10;
  }

  size_t len = ptr - buffer;
  *ptr = '\0';

  // Reverse the string in-place
  ptr--;
  while (start < ptr) {
    char temp = *start;
    *start++ = *ptr;
    *ptr-- = temp;
  }

  return len;
}

int ReplyWithUint64(RedisModuleCtx* ctx, uint64_t value) {
  size_t len = uint64_to_string(value, REPLY_UINT64_BUFFER);
  if (RedisModule_ReplyWithBigNumber != NULL) {
    return RedisModule_ReplyWithBigNumber(ctx, REPLY_UINT64_BUFFER, len);
  }

  if (value <= LLONG_MAX) {
    return RedisModule_ReplyWithLongLong(ctx, (long long) value);
  }

  return RedisModule_ReplyWithStringBuffer(ctx, REPLY_UINT64_BUFFER, len);
}

bool StrToUInt32(const RedisModuleString* str, uint32_t* ull) {
  long long value;

  if ((RedisModule_StringToLongLong(str, &value) != REDISMODULE_OK) || value < 0 || value > UINT32_MAX) {
    return false;
  }

  *ull = (uint32_t) value;
  return true;
}

bool StrToUInt64(const RedisModuleString* str, uint64_t* ull) {
  unsigned long long value;

  if ((RedisModule_StringToULongLong(str, &value) != REDISMODULE_OK) || value > UINT64_MAX) {
    return false;
  }

  *ull = (uint64_t) value;
  return true;
}

bool StrToBool(const RedisModuleString* str, bool* ull) {
  long long value;

  if ((RedisModule_StringToLongLong(str, &value) != REDISMODULE_OK) || value < 0 || value > 1) {
    return false;
  }

  *ull = (char) value;
  return true;
}
