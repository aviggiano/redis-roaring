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