#include "redismodule.h"
#include <limits.h>

//static RedisModuleType *RBitmapType;

/* === Internal data structure === */
typedef struct r_bitmap {
    size_t size;
    char *array;
} RBitmap;

RBitmap* construct() {
    RBitmap* rbitmap = RedisModule_Alloc(10);
    return rbitmap;
}

/* === RBitmap type methods === */

/* === RBitmap type commands === */

/**
 * R.SETBIT <key> <offset> <value>
 * */
int RSetBitCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModule_AutoMemory(ctx);
    REDISMODULE_NOT_USED(argv);

    RedisModule_ReplyWithSimpleString(ctx, "OK");

    return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {
    // Register the module itself
    if (RedisModule_Init(ctx, "REROARING", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    // register our commands
    if (RedisModule_CreateCommand(ctx, "R.SETBIT", RSetBitCommand, "write", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
