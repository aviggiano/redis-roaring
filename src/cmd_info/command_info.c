#include "command_info.h"

const RedisModuleCommandInfo* FindRedisRoaringCommandInfo(const char* name) {
  const RedisModuleCommandInfo* info = GetRootCommandInfo(name);
  if (info != NULL) {
    return info;
  }

  info = GetRCommandInfo(name);
  if (info != NULL) {
    return info;
  }

  return GetR64CommandInfo(name);
}
