#pragma once

#include "redismodule.h"

int RegisterRootCommandInfos(RedisModuleCtx* ctx);
int RegisterRCommandInfos(RedisModuleCtx* ctx);
int RegisterR64CommandInfos(RedisModuleCtx* ctx);

const RedisModuleCommandInfo* GetRootCommandInfo(const char* name);
const RedisModuleCommandInfo* GetRCommandInfo(const char* name);
const RedisModuleCommandInfo* GetR64CommandInfo(const char* name);
const RedisModuleCommandInfo* FindRedisRoaringCommandInfo(const char* name);
