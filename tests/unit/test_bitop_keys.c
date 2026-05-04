#include "bitop_keys.h"
#include "../test-utils.h"

typedef struct {
  size_t count;
  BitOpKeyPosition keys[16];
} BitOpKeyRecorder;

static void record_bitop_key(void* ctx, int pos, int flags) {
  BitOpKeyRecorder* recorder = ctx;
  ASSERT(recorder->count < (sizeof(recorder->keys) / sizeof(recorder->keys[0])), "recorded too many BITOP keys");
  recorder->keys[recorder->count++] = (BitOpKeyPosition){
      .pos = pos,
      .flags = flags
  };
}

void test_bitop_keys() {
  DESCRIBE("bitop key discovery")
  {
    IT("Should report destination and source keys for variadic operations")
    {
      BitOpKeyRecorder recorder = {0};
      size_t count = BitOpForEachKeyPosition("OR", 5, &recorder, record_bitop_key);

      ASSERT(count == 3, "expected 3 keys, got %zu", count);
      ASSERT(recorder.count == 3, "expected 3 recorded keys, got %zu", recorder.count);
      ASSERT(recorder.keys[0].pos == 2, "expected destination at position 2");
      ASSERT(recorder.keys[0].flags == (REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_INSERT), "unexpected destination flags");
      ASSERT(recorder.keys[1].pos == 3, "expected first source at position 3");
      ASSERT(recorder.keys[1].flags == (REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS), "unexpected source flags");
      ASSERT(recorder.keys[2].pos == 4, "expected second source at position 4");
      ASSERT(recorder.keys[2].flags == (REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS), "unexpected source flags");
    }

    IT("Should report only destination and source key for NOT operations")
    {
      BitOpKeyRecorder recorder = {0};
      size_t count = BitOpForEachKeyPosition("NOT", 5, &recorder, record_bitop_key);

      ASSERT(count == 2, "expected 2 keys, got %zu", count);
      ASSERT(recorder.count == 2, "expected 2 recorded keys, got %zu", recorder.count);
      ASSERT(recorder.keys[0].pos == 2, "expected NOT destination at position 2");
      ASSERT(recorder.keys[1].pos == 3, "expected NOT source at position 3");
    }

    IT("Should reject unsupported operations and invalid arity")
    {
      ASSERT(BitOpForEachKeyPosition("NOOP", 5, NULL, NULL) == 0, "invalid operation should not report keys");
      ASSERT(BitOpForEachKeyPosition("OR", 4, NULL, NULL) == 0, "variadic BITOP needs at least 3 keys");
      ASSERT(BitOpForEachKeyPosition("NOT", 6, NULL, NULL) == 0, "NOT should reject extra non-key arguments");
    }
  }
}
