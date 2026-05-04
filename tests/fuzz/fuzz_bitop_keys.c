#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "../../src/bitop_keys.h"
#include "fuzz_common.h"

typedef struct {
  size_t count;
  BitOpKeyPosition keys[256];
} FuzzBitOpKeyRecorder;

static void fuzz_record_bitop_key(void* ctx, int pos, int flags) {
  FuzzBitOpKeyRecorder* recorder = ctx;
  fuzz_require(recorder->count < (sizeof(recorder->keys) / sizeof(recorder->keys[0])));
  recorder->keys[recorder->count++] = (BitOpKeyPosition){
      .pos = pos,
      .flags = flags
  };
}

static size_t fuzz_expected_bitop_key_count(const char* operation, int argc) {
  if (strcmp(operation, "NOT") == 0) {
    return (argc >= 4 && argc <= 5) ? 2 : 0;
  }

  if (!BitOpIsVariadicOperation(operation) || argc < 5) {
    return 0;
  }

  return (size_t)(argc - 2);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 3) {
    return 0;
  }

  FuzzInput input;
  fuzz_input_init(&input, data, size);

  static const char* operations[] = {
      "AND", "OR", "XOR", "NOT", "ANDOR", "ONE", "DIFF", "DIFF1", "NOOP"
  };

  const char* operation = operations[fuzz_consume_u8(&input) % (sizeof(operations) / sizeof(operations[0]))];
  int argc = (int)fuzz_consume_u32_in_range(&input, 0, 32);

  FuzzBitOpKeyRecorder recorder = {0};
  size_t count = BitOpForEachKeyPosition(operation, argc, &recorder, fuzz_record_bitop_key);
  size_t expected = fuzz_expected_bitop_key_count(operation, argc);

  fuzz_require(count == expected);
  fuzz_require(recorder.count == expected);

  for (size_t i = 0; i < recorder.count; i++) {
    BitOpKeyPosition key = recorder.keys[i];

    fuzz_require(key.pos >= 2);
    fuzz_require(key.pos < argc);

    if (i == 0) {
      fuzz_require(key.pos == 2);
      fuzz_require(key.flags == (REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_INSERT));
    } else {
      fuzz_require(key.flags == (REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS));
      fuzz_require(key.pos == recorder.keys[i - 1].pos + 1);
    }
  }

  if (strcmp(operation, "NOT") == 0 && expected == 2) {
    fuzz_require(recorder.keys[0].pos == 2);
    fuzz_require(recorder.keys[1].pos == 3);
  }

  return 0;
}
