#ifndef REDIS_ROARING_DATA_STRUCTURE_H
#define REDIS_ROARING_DATA_STRUCTURE_H

#include "roaring.h"

typedef roaring_bitmap_t Bitmap;

Bitmap* bitmap_alloc();
void bitmap_free(Bitmap* bitmap);
uint64_t bitmap_get_cardinality(Bitmap* bitmap);
void bitmap_setbit(Bitmap* bitmap, uint32_t offset, char value);
char bitmap_getbit(Bitmap* bitmap, uint32_t offset);
/**
 * Gets the n-th element of the set.
 *
 * @param bitmap - the bitmap set
 * @param n - the position of the element on the set
 *
 * @return the n-th element of the bitmap set
 *
 * @example set {1, 10, 100, 1000} and n=1 returns 1, n=3 returns 100, n=0 returns -1, n=5 returns -1
 */
int64_t bitmap_get_nth_element(Bitmap* bitmap, uint64_t n);
Bitmap* bitmap_or(uint32_t n, const Bitmap** bitmaps);
Bitmap* bitmap_and(uint32_t n, const Bitmap** bitmaps);
Bitmap* bitmap_xor(uint32_t n, const Bitmap** bitmaps);
/**
 * Returns a bitmap equals to the inverted set of first bitmap of the `bitmaps` array,
 * ignoring the first parameter of the function.  * The purpose of this function is to
 * avoid code repetition on the redis module.
 *
 * @param n - unused parameter
 * @param bitmaps - array containing a bitmap to be flipped
 *
 * @return a NOT of bitmaps[0]
 */
Bitmap* bitmap_not_array(uint32_t unused, const Bitmap** bitmaps);
Bitmap* bitmap_not(const Bitmap* bitmap);
Bitmap* bitmap_from_int_array(size_t n, const uint32_t* array);
uint32_t* bitmap_get_int_array(Bitmap* bitmap, size_t* n);
void bitmap_free_int_array(uint32_t* array);
Bitmap* bitmap_from_bit_array(size_t n, const char* array);
char* bitmap_get_bit_array(Bitmap* bitmap, size_t* n);
void bitmap_free_bit_array(char* array);

#endif
