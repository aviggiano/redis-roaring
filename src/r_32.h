#pragma once

#include "redismodule.h"
#include "data-structure.h"

#define BITMAP_ENCODING_VERSION 1
#define BITMAP_MAX_RANGE_SIZE 100000000

extern RedisModuleType* BitmapType;
extern Bitmap* BITMAP_NILL;

int R32Module_onLoad(RedisModuleCtx* ctx, RedisModuleString** argv, int argc);
void R32Module_onShutdown(RedisModuleCtx* ctx, RedisModuleEvent e, uint64_t sub, void* data);
