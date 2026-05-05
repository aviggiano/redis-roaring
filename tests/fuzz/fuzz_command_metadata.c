#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../src/bitop_keys.h"
#include "../../src/cmd_info/command_info.h"
#include "fuzz_command_manifest.h"
#include "fuzz_common.h"

#define MAX_EXPECTED_KEYS 16

typedef struct {
  size_t count;
  BitOpKeyPosition keys[MAX_EXPECTED_KEYS];
} FuzzKeyRecorder;

static void fuzz_record_key(void* ctx, int pos, int flags) {
  FuzzKeyRecorder* recorder = ctx;
  fuzz_require(recorder->count < MAX_EXPECTED_KEYS);
  recorder->keys[recorder->count++] = (BitOpKeyPosition){
      .pos = pos,
      .flags = flags
  };
}

static const FuzzCommandDescriptor* fuzz_find_descriptor(const char* name) {
  for (size_t i = 0; i < fuzz_command_descriptor_count(); i++) {
    if (strcmp(FUZZ_COMMAND_DESCRIPTORS[i].name, name) == 0) {
      return &FUZZ_COMMAND_DESCRIPTORS[i];
    }
  }

  return NULL;
}

static size_t fuzz_count_key_specs(const RedisModuleCommandInfo* info) {
  size_t count = 0;
  while (info->key_specs[count].flags != 0
      || info->key_specs[count].begin_search_type != REDISMODULE_KSPEC_BS_INVALID
      || info->key_specs[count].find_keys_type != REDISMODULE_KSPEC_FK_OMITTED) {
    count++;
  }
  return count;
}

static bool fuzz_command_accepts_argc(const FuzzCommandDescriptor* desc, int argc) {
  if (argc < desc->min_argc) {
    return false;
  }

  return desc->max_argc < 0 || argc <= desc->max_argc;
}

static int fuzz_pick_argc(const FuzzCommandDescriptor* desc, FuzzInput* input) {
  static const int unbounded_offsets[] = {-1, 0, 1, 4, 8};
  static const int bounded_offsets[] = {-1, 0, 1};

  if (desc->shape == FUZZ_COMMAND_BITOP) {
    return (int)fuzz_consume_u32_in_range(input, 0, 12);
  }

  if (desc->max_argc < 0) {
    int candidate = desc->min_argc + unbounded_offsets[fuzz_consume_u8(input) % (sizeof(unbounded_offsets) / sizeof(unbounded_offsets[0]))];
    return candidate < 0 ? 0 : candidate;
  }

  int branch = (int)(fuzz_consume_u8(input) % 5);
  int candidate = 0;
  if (branch < 3) {
    candidate = desc->min_argc + bounded_offsets[branch];
  } else if (branch == 3) {
    candidate = desc->max_argc;
  } else {
    candidate = desc->max_argc + 1;
  }

  return candidate < 0 ? 0 : candidate;
}

static size_t fuzz_model_bitop_keys(const char* operation, int argc, BitOpKeyPosition* out, size_t cap) {
  if (operation == NULL) {
    return 0;
  }

  if (strcmp(operation, "NOT") == 0) {
    if (argc < 4 || argc > 5) {
      return 0;
    }

    fuzz_require(cap >= 2);
    out[0] = (BitOpKeyPosition){.pos = 2, .flags = REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_INSERT};
    out[1] = (BitOpKeyPosition){.pos = 3, .flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS};
    return 2;
  }

  if (!BitOpIsVariadicOperation(operation) || argc < 5) {
    return 0;
  }

  size_t expected = (size_t)(argc - 2);
  fuzz_require(expected <= cap);
  out[0] = (BitOpKeyPosition){.pos = 2, .flags = REDISMODULE_CMD_KEY_RW | REDISMODULE_CMD_KEY_INSERT};
  for (size_t i = 1; i < expected; i++) {
    out[i] = (BitOpKeyPosition){
        .pos = (int)(2 + i),
        .flags = REDISMODULE_CMD_KEY_RO | REDISMODULE_CMD_KEY_ACCESS
    };
  }
  return expected;
}

static void fuzz_require_keys_in_range(const BitOpKeyPosition* keys, size_t count, int argc) {
  for (size_t i = 0; i < count; i++) {
    fuzz_require(keys[i].pos >= 1);
    fuzz_require(keys[i].pos < argc);
    if (i > 0) {
      fuzz_require(keys[i].pos > keys[i - 1].pos);
    }
  }
}

static void fuzz_require_key_specs_match(const RedisModuleCommandInfo* info, const FuzzCommandDescriptor* desc) {
  fuzz_require(info != NULL);
  fuzz_require(info->arity == desc->redis_arity);
  fuzz_require(info->key_specs != NULL);

  size_t actual_count = fuzz_count_key_specs(info);
  fuzz_require(actual_count == desc->key_spec_count);

  for (size_t i = 0; i < actual_count; i++) {
    const RedisModuleCommandKeySpec* actual = &info->key_specs[i];
    const FuzzKeySpecExpectation* expected = &desc->key_specs[i];

    fuzz_require(actual->begin_search_type == REDISMODULE_KSPEC_BS_INDEX);
    fuzz_require(actual->find_keys_type == REDISMODULE_KSPEC_FK_RANGE);
    fuzz_require(actual->bs.index.pos == expected->begin_pos);
    fuzz_require(actual->fk.range.lastkey == expected->lastkey);
    fuzz_require(actual->fk.range.keystep == expected->keystep);
    fuzz_require(actual->fk.range.limit == expected->limit);
    fuzz_require((int)actual->flags == expected->flags);
  }
}

static void fuzz_require_parity_metadata(const FuzzCommandDescriptor* desc, const RedisModuleCommandInfo* info) {
  if (desc->counterpart == NULL) {
    return;
  }

  const FuzzCommandDescriptor* counterpart_desc = fuzz_find_descriptor(desc->counterpart);
  const RedisModuleCommandInfo* counterpart_info = FindRedisRoaringCommandInfo(desc->counterpart);

  fuzz_require(counterpart_desc != NULL);
  fuzz_require(counterpart_info != NULL);
  fuzz_require(counterpart_info->arity == info->arity);
  fuzz_require(fuzz_count_key_specs(counterpart_info) == fuzz_count_key_specs(info));

  for (size_t i = 0; i < desc->key_spec_count; i++) {
    const RedisModuleCommandKeySpec* lhs = &info->key_specs[i];
    const RedisModuleCommandKeySpec* rhs = &counterpart_info->key_specs[i];

    fuzz_require(lhs->begin_search_type == rhs->begin_search_type);
    fuzz_require(lhs->find_keys_type == rhs->find_keys_type);
    fuzz_require(lhs->bs.index.pos == rhs->bs.index.pos);
    fuzz_require(lhs->fk.range.lastkey == rhs->fk.range.lastkey);
    fuzz_require(lhs->fk.range.keystep == rhs->fk.range.keystep);
    fuzz_require(lhs->fk.range.limit == rhs->fk.range.limit);
    fuzz_require(lhs->flags == rhs->flags);
  }

  fuzz_require(counterpart_desc->redis_arity == desc->redis_arity);
  fuzz_require(counterpart_desc->shape == desc->shape);
}

static void fuzz_check_fixed_command(const FuzzCommandDescriptor* desc, int argc) {
  if (!fuzz_command_accepts_argc(desc, argc)) {
    return;
  }

  BitOpKeyPosition expected[MAX_EXPECTED_KEYS];
  fuzz_require(desc->expanded_key_count > 0);
  fuzz_require(desc->expanded_key_count <= MAX_EXPECTED_KEYS);
  for (size_t i = 0; i < desc->expanded_key_count; i++) {
    expected[i] = (BitOpKeyPosition){
        .pos = desc->expanded_keys[i].pos,
        .flags = desc->expanded_keys[i].flags
    };
  }

  size_t count = desc->expanded_key_count;
  fuzz_require_keys_in_range(expected, count, argc);
}

static void fuzz_check_bitop_command(const FuzzCommandDescriptor* desc, int argc, FuzzInput* input) {
  static const char* operations[] = {
      "AND", "OR", "XOR", "NOT", "ANDOR", "ONE", "DIFF", "DIFF1", "NOOP"
  };

  const char* operation = operations[fuzz_consume_u8(input) % (sizeof(operations) / sizeof(operations[0]))];
  BitOpKeyPosition modeled[MAX_EXPECTED_KEYS];
  FuzzKeyRecorder recorder = {0};

  size_t actual = BitOpForEachKeyPosition(operation, argc, &recorder, fuzz_record_key);
  size_t expected = fuzz_model_bitop_keys(operation, argc, modeled, MAX_EXPECTED_KEYS);

  fuzz_require(actual == expected);
  fuzz_require(recorder.count == expected);
  for (size_t i = 0; i < expected; i++) {
    fuzz_require(recorder.keys[i].pos == modeled[i].pos);
    fuzz_require(recorder.keys[i].flags == modeled[i].flags);
  }

  if (expected > 0) {
    fuzz_require_keys_in_range(modeled, expected, argc);
  }

  fuzz_require(desc->min_argc == 4);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0) {
    return 0;
  }

  FuzzInput input;
  fuzz_input_init(&input, data, size);

  const FuzzCommandDescriptor* desc =
      &FUZZ_COMMAND_DESCRIPTORS[fuzz_consume_u8(&input) % fuzz_command_descriptor_count()];
  const RedisModuleCommandInfo* info = FindRedisRoaringCommandInfo(desc->name);

  fuzz_require_key_specs_match(info, desc);
  fuzz_require_parity_metadata(desc, info);

  int argc = fuzz_pick_argc(desc, &input);
  if (desc->shape == FUZZ_COMMAND_FIXED) {
    fuzz_check_fixed_command(desc, argc);
  } else {
    fuzz_check_bitop_command(desc, argc, &input);
  }

  return 0;
}
