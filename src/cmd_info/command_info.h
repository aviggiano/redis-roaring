#pragma once

#include "redismodule.h"

int RegisterRCommandInfos(RedisModuleCtx* ctx);
int RegisterR64CommandInfos(RedisModuleCtx* ctx);
