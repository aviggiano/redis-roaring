#ifndef REDIS_ROARING_DATA_STRUCTURE_H
#define REDIS_ROARING_DATA_STRUCTURE_H

#include "roaring.h"

typedef roaring_bitmap_t Bitmap;
typedef roaring_statistics_t Bitmap_statistics;

Bitmap* bitmap_alloc();
void bitmap_free(Bitmap* bitmap);
uint64_t bitmap_get_cardinality(const Bitmap* bitmap);
void bitmap_setbit(Bitmap* bitmap, uint32_t offset, bool value);
bool bitmap_getbit(const Bitmap* bitmap, uint32_t offset);
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
int64_t bitmap_get_nth_element_present(const Bitmap* bitmap, uint64_t n);
/**
 * Gets the n-th element not present in the set
 *
 * @param bitmap - the bitmap set
 * @param n - the position of the element not present in the set
 *
 * @return the n-th non-existent element of the bitmap set
 *
 * @example set {1, 2, 4, 5, 7} and n=1 returns 0, n=2 returns 3, n=3 returns 6, n=5 returns 9
 */
int64_t bitmap_get_nth_element_not_present(const Bitmap* bitmap, uint64_t n);
/**
 * Same as `bitmap_get_nth_element_not_present` but slower. Useful for cross validation.
 */
int64_t bitmap_get_nth_element_not_present_slow(const Bitmap* bitmap, uint64_t n);
Bitmap* bitmap_or(uint32_t n, const Bitmap** bitmaps);
Bitmap* bitmap_and(uint32_t n, const Bitmap** bitmaps);
Bitmap* bitmap_xor(uint32_t n, const Bitmap** bitmaps);
Bitmap* bitmap_flip(const Bitmap* bitmap, uint32_t end); 
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
uint32_t* bitmap_get_int_array(const Bitmap* bitmap, size_t* n);
uint32_t* bitmap_range_int_array(const Bitmap* bitmap, size_t offset, size_t n);
void bitmap_free_int_array(uint32_t* array);
/**
 * Creates a Bitmap from a buffer composed from ASCII '0's and '1's
 *
 * @param size - the buffer size
 * @param array - the buffer bit array
 * @return a Bitmap corresponding to the bit array
 */
Bitmap* bitmap_from_bit_array(size_t size, const char* array);
/**
 * Creates a buffer string composed from ASCII '0's and '1's from a Bitmap
 *
 * @param bitmap - to be converted to a buffer string
 * @param size - the buffer size
 * @return the buffer bit array
 */
char* bitmap_get_bit_array(const Bitmap* bitmap, size_t* size);
void bitmap_free_bit_array(char* array);
/**
 * Creates a roaring bitmap filled with a range of numbers
 *
 * @param from - the first element
 * @param to - the last element
 * @return the roaring bitmap
 */
Bitmap* bitmap_from_range(uint64_t from, uint64_t to);
bool bitmap_is_empty(const Bitmap* bitmap);
uint32_t bitmap_min(const Bitmap* bitmap);
uint32_t bitmap_max(const Bitmap* bitmap);
void bitmap_optimize(Bitmap* bitmap);
void bitmap_statistics(const Bitmap *bitmap, Bitmap_statistics *stat);

#endif
