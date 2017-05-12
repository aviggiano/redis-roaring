#include "data_structure.h"

/* === Internal data structure === */

Bitmap* bitmap_alloc() {
  return roaring_bitmap_create();
}

void bitmap_free(Bitmap* bitmap) {
  roaring_bitmap_free(bitmap);
}

void bitmap_setbit(Bitmap* bitmap, uint32_t offset, char value) {
  if (value == 0) {
    roaring_bitmap_remove(bitmap, offset);
  }
  else {
    roaring_bitmap_add(bitmap, offset);
  }
}

char bitmap_getbit(Bitmap* bitmap, uint32_t offset) {
  return roaring_bitmap_contains(bitmap, offset) == true;
}

Bitmap* bitmap_or(uint32_t n, const Bitmap** bitmaps) {
  return roaring_bitmap_or_many_heap(n, bitmaps);
}

Bitmap* bitmap_and(uint32_t n, const Bitmap** bitmaps) {
  Bitmap* result = roaring_bitmap_copy(bitmaps[0]);
  for (uint32_t i = 1; i < n; i++) {
    roaring_bitmap_and_inplace(result, bitmaps[i]);
  }
  return result;
}

Bitmap* bitmap_xor(uint32_t n, const Bitmap** bitmaps) {
  return roaring_bitmap_xor_many(n, bitmaps);
}

Bitmap* bitmap_not_array(uint32_t unused, const Bitmap** bitmaps) {
  (void) (unused);
  uint32_t last = roaring_bitmap_maximum(bitmaps[0]);
  return roaring_bitmap_flip(bitmaps[0], 0, last + 1);
}

Bitmap* bitmap_not(const Bitmap* bitmap) {
  const Bitmap* bitmaps[] = {bitmap};
  return bitmap_not_array(1, bitmaps);
}