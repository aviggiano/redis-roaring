#include "data-structure.h"

/* === Internal data structure === */

Bitmap* bitmap_alloc() {
  return roaring_bitmap_create();
}

void bitmap_free(Bitmap* bitmap) {
  roaring_bitmap_free(bitmap);
}

uint64_t bitmap_get_cardinality(Bitmap* bitmap) {
  return roaring_bitmap_get_cardinality(bitmap);
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

int64_t bitmap_get_nth_element(Bitmap* bitmap, uint64_t n) {
  roaring_uint32_iterator_t* iterator = roaring_create_iterator(bitmap);
  int64_t element = -1;
  for (uint64_t i = 1; iterator->has_value; i++) {
    if (i == n) {
      element = iterator->current_value;
      break;
    }
    roaring_advance_uint32_iterator(iterator);
  }
  roaring_free_uint32_iterator(iterator);
  return element;
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
