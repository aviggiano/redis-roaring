#ifndef REDIS_ROARING_TYPE_H
#define REDIS_ROARING_TYPE_H

void BitmapRdbSave(RedisModuleIO* rdb, void* value);
void* BitmapRdbLoad(RedisModuleIO* rdb, int encver);
void BitmapAofRewrite(RedisModuleIO* aof, RedisModuleString* key, void* value);
size_t BitmapMemUsage(const void* value);
void BitmapFree(void* value);

#endif
