#pragma once

#include "redismodule.h"
#include "data-structure.h"

#define BITMAP64_ENCODING_VERSION 1
#define BITMAP64_MAX_RANGE_SIZE 100000000

extern RedisModuleType* Bitmap64Type;
extern Bitmap64* BITMAP64_NILL;

int R64Module_onLoad(RedisModuleCtx* ctx, RedisModuleString** argv, int argc);
void R64Module_onShutdown(RedisModuleCtx* ctx, RedisModuleEvent e, uint64_t sub, void* data);
