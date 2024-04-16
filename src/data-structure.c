#include "data-structure.h"

#include "roaring.h"

/* === Internal data structure === */

Bitmap* bitmap_alloc() {
  return roaring_bitmap_create();
}

void bitmap_free(Bitmap* bitmap) {
  roaring_bitmap_free(bitmap);
}

uint64_t bitmap_get_cardinality(const Bitmap* bitmap) {
  return roaring_bitmap_get_cardinality(bitmap);
}

void bitmap_setbit(Bitmap* bitmap, uint32_t offset, bool value) {
  if (!value) {
    roaring_bitmap_remove(bitmap, offset);
  } else {
    roaring_bitmap_add(bitmap, offset);
  }
}

bool bitmap_getbit(const Bitmap* bitmap, uint32_t offset) {
  return roaring_bitmap_contains(bitmap, offset);
}

int64_t bitmap_get_nth_element_present(const Bitmap* bitmap, uint64_t n) {
  roaring_uint32_iterator_t* iterator = roaring_iterator_create(bitmap);
  int64_t element = -1;
  for (uint64_t i = 1; iterator->has_value; i++) {
    if (i == n) {
      element = iterator->current_value;
      break;
    }
    roaring_uint32_iterator_advance(iterator);
  }
  roaring_uint32_iterator_free(iterator);
  return element;
}

int64_t bitmap_get_nth_element_not_present(const Bitmap* bitmap, uint64_t n) {
  roaring_uint32_iterator_t* iterator = roaring_iterator_create(bitmap);
  int64_t element = -1;
  int64_t last = -1;
  for (uint64_t i = 1; iterator->has_value; i++) {
    int64_t current = iterator->current_value;
    int64_t step = current - last;
    if (n < step) {
      element = last + n;
      break;
    } else {
      n -= (step - 1);
    }
    last = current;
    roaring_uint32_iterator_advance(iterator);
  }
  roaring_uint32_iterator_free(iterator);
  return element;
}

int64_t bitmap_get_nth_element_not_present_slow(const Bitmap* bitmap, uint64_t n) {
  roaring_bitmap_t* inverted_bitmap = bitmap_not(bitmap);
  int64_t element = bitmap_get_nth_element_present(inverted_bitmap, n);
  roaring_bitmap_free(inverted_bitmap);
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

Bitmap* bitmap_flip(const Bitmap* bitmap, uint32_t end) {
  return roaring_bitmap_flip(bitmap, 0, end);
}


Bitmap* bitmap_not(const Bitmap* bitmap) {
  const Bitmap* bitmaps[] = {bitmap};
  return bitmap_not_array(1, bitmaps);
}

Bitmap* bitmap_from_int_array(size_t n, const uint32_t* array) {
  return roaring_bitmap_of_ptr(n, array);
}

uint32_t* bitmap_get_int_array(const Bitmap* bitmap, size_t* n) {
  *n = roaring_bitmap_get_cardinality(bitmap);
  uint32_t* ans = malloc(sizeof(*ans) * (*n));
  roaring_bitmap_to_uint32_array(bitmap, ans);
  return ans;
}

uint32_t* bitmap_range_int_array(const Bitmap* bitmap, size_t offset, size_t n) {
  uint32_t* ans = calloc(n, sizeof(*ans));
  roaring_bitmap_range_uint32_array(bitmap, offset, n, ans);
  return ans;
}

void bitmap_free_int_array(uint32_t* array) {
  free(array);
}

Bitmap* bitmap_from_bit_array(size_t size, const char* array) {
  Bitmap* bitmap = bitmap_alloc();
  for (size_t i = 0; i < size; i++) {
    if (array[i] == '1') {
      roaring_bitmap_add(bitmap, (int32_t) i);
    }
  }
  return bitmap;
}

char* bitmap_get_bit_array(const Bitmap* bitmap, size_t* size) {
  *size = roaring_bitmap_maximum(bitmap) + 1;
  char* ans = malloc(*size + 1);
  memset(ans, '0', *size);
  ans[*size] = '\0';

  roaring_uint32_iterator_t* iterator = roaring_iterator_create(bitmap);
  while (iterator->has_value) {
    ans[iterator->current_value] = '1';
    roaring_uint32_iterator_advance(iterator);
  }
  roaring_uint32_iterator_free(iterator);

  return ans;
}

void bitmap_free_bit_array(char* array) {
  free(array);
}

Bitmap* bitmap_from_range(uint64_t from, uint64_t to) {
  return roaring_bitmap_from_range(from, to, 1);
}

bool bitmap_is_empty(const Bitmap* bitmap) {
  return roaring_bitmap_is_empty(bitmap);
}

uint32_t bitmap_min(const Bitmap* bitmap) {
  return roaring_bitmap_minimum(bitmap);
}

uint32_t bitmap_max(const Bitmap* bitmap) {
  return roaring_bitmap_maximum(bitmap);
}

void bitmap_optimize(Bitmap* bitmap) {
  roaring_bitmap_run_optimize(bitmap);
}

void bitmap_statistics(const Bitmap* bitmap, Bitmap_statistics* stat) {
  roaring_bitmap_statistics(bitmap, stat);
}