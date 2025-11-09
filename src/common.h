#pragma once

#define SetCommandAcls(ctx, cmd, acls)                                                             \
    do {                                                                                           \
        RedisModuleCommand *command = RedisModule_GetCommand(ctx, cmd);                            \
        if (command == NULL) {                                                                     \
            RedisModule_Log(ctx, "error", "Failed to get command %s", cmd);                        \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
        if (RedisModule_SetCommandACLCategories(command, acls) != REDISMODULE_OK) {                \
            RedisModule_Log(ctx, "error", "Failed to set ACL categories %s for command %s", acls,  \
                            cmd);                                                                  \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
    } while (0)

#define SetCommandInfo(ctx, name, info)                                              \
    do {                                                                             \
        RedisModuleCommand *cmd = RedisModule_GetCommand(ctx, name);                \
        if (!cmd) { \
          RedisModule_Log(ctx, "warning", "Failed to set %s command info (not exists)", name); \
          return REDISMODULE_ERR; \
        }                                           \
        if (RedisModule_SetCommandInfo(cmd, info) == REDISMODULE_ERR) {            \
            RedisModule_Log(ctx, "warning", "Failed to set %s command info", name); \
            return REDISMODULE_ERR;                                                  \
        }                                                                            \
    } while(0)

#define RegisterCommandWithModesAndAcls(ctx, cmd, f, mode, acls)                                   \
    do {                                                                                           \
        if (RedisModule_CreateCommand(ctx, cmd, f, mode, 1, 1, 1) != REDISMODULE_OK) {             \
            RedisModule_Log(ctx, "error", "Failed to create command %s", cmd);                     \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
        SetCommandAcls(ctx, cmd, acls);                                                            \
    } while (0)

#define RegisterAclCategory(ctx, module)                                                           \
    if (RedisModule_AddACLCategory(ctx, module) != REDISMODULE_OK) {                               \
        RedisModule_Log(ctx, "error", "Failed to add ACL category %s", module);                    \
        return REDISMODULE_ERR;                                                                    \
    }
