#ifndef REDIS_ROARING_DATA_STRUCTURE_H
#define REDIS_ROARING_DATA_STRUCTURE_H

#include "roaring.h"

typedef roaring_bitmap_t Bitmap;

Bitmap* bitmap_alloc();
void bitmap_free(Bitmap* bitmap);
uint64_t bitmap_get_cardinality(Bitmap* bitmap);
void bitmap_setbit(Bitmap* bitmap, uint32_t offset, char value);
char bitmap_getbit(Bitmap* bitmap, uint32_t offset);
Bitmap* bitmap_or(uint32_t n, const Bitmap** bitmaps);
Bitmap* bitmap_and(uint32_t n, const Bitmap** bitmaps);
Bitmap* bitmap_xor(uint32_t n, const Bitmap** bitmaps);
/**
 * @param n - unused parameter
 * @param bitmaps - array containing a bitmap to be flipped
 *
 * Ignores the first parameter and inverts the first bitmap of the `bitmaps` array
 * The purpose of this function is to avoid code repetition on the redis module
 *
 * @return a NOT of bitmaps[0]
 */
Bitmap* bitmap_not_array(uint32_t unused, const Bitmap** bitmaps);
Bitmap* bitmap_not(const Bitmap* bitmap);

#endif
