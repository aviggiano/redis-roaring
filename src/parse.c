#include "parse.h"
#include <limits.h>
#include <stdio.h>

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
  if (value <= LLONG_MAX) {
    return RedisModule_ReplyWithLongLong(ctx, (long long) value);
  }

  return RedisModule_ReplyWithStringBuffer(ctx, REPLY_UINT64_BUFFER, len);
}

int ReplyWithErrorFmt(RedisModuleCtx* ctx, const char* fmt, ...) {
  char buffer[256];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, ap);
  va_end(ap);

  return RedisModule_ReplyWithError(ctx, buffer);
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
  size_t len;
  const char* s = RedisModule_StringPtrLen(str, &len);
  if (len == 0) {
    return false;
  }

  if (s[0] == '-') {
    return false;
  }

  size_t i = 0;
  if (s[0] == '+') {
    if (len == 1) {
      return false;
    }
    i = 1;
  }

  uint64_t value = 0;
  for (; i < len; i++) {
    unsigned char c = (unsigned char) s[i];
    if (c < '0' || c > '9') {
      return false;
    }

    uint64_t digit = (uint64_t) (c - '0');
    if (value > (UINT64_MAX - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
  }

  *ull = value;
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
