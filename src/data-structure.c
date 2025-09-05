#include "data-structure.h"

#include "roaring.h"
#include "rmalloc.h"

/* === Internal data structure === */

Bitmap* bitmap_alloc() {
  return roaring_bitmap_create();
}

Bitmap64* bitmap64_alloc() {
  return roaring64_bitmap_create();
}

void bitmap_free(Bitmap* bitmap) {
  roaring_bitmap_free(bitmap);
}

void bitmap64_free(Bitmap64* bitmap) {
  roaring64_bitmap_free(bitmap);
}

uint64_t bitmap_get_cardinality(const Bitmap* bitmap) {
  return roaring_bitmap_get_cardinality(bitmap);
}

uint64_t bitmap64_get_cardinality(const Bitmap64* bitmap) {
  return roaring64_bitmap_get_cardinality(bitmap);
}

void bitmap_setbit(Bitmap* bitmap, uint32_t offset, bool value) {
  if (!value) {
    roaring_bitmap_remove(bitmap, offset);
  } else {
    roaring_bitmap_add(bitmap, offset);
  }
}

void bitmap64_setbit(Bitmap64* bitmap, uint64_t offset, bool value) {
  if (!value) {
    roaring64_bitmap_remove(bitmap, offset);
  } else {
    roaring64_bitmap_add(bitmap, offset);
  }
}

bool bitmap_getbit(const Bitmap* bitmap, uint32_t offset) {
  return roaring_bitmap_contains(bitmap, offset);
}

bool bitmap64_getbit(const Bitmap64* bitmap, uint64_t offset) {
  return roaring64_bitmap_contains(bitmap, offset);
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

uint64_t bitmap64_get_nth_element_present(const Bitmap64* bitmap, uint64_t n, bool* found) {
  roaring64_iterator_t* iterator = roaring64_iterator_create(bitmap);
  uint64_t element = 0;
  for (uint64_t i = 1; roaring64_iterator_has_value(iterator); i++) {
    if (i == n) {
      element = roaring64_iterator_value(iterator);
      *found = true;
      break;
    }
    roaring64_iterator_advance(iterator);
  }
  roaring64_iterator_free(iterator);
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

uint64_t bitmap64_get_nth_element_not_present(const Bitmap64* bitmap, uint64_t n, bool* found) {
  roaring64_iterator_t* iterator = roaring64_iterator_create(bitmap);
  uint64_t element = 0;
  uint64_t last = UINT64_MAX;

  for (uint64_t i = 1; roaring64_iterator_has_value(iterator); i++) {
    uint64_t current = roaring64_iterator_value(iterator);
    uint64_t step = (last == UINT64_MAX) ? current + 1 : current - last;

    if (n < step) {
      element = (last == UINT64_MAX) ? n : last + n;
      *found = true;
      break;
    } else {
      n -= (step - 1);
    }
    last = current;
    roaring64_iterator_advance(iterator);
  }

  roaring64_iterator_free(iterator);
  return element;
}

int64_t bitmap_get_nth_element_not_present_slow(const Bitmap* bitmap, uint64_t n) {
  roaring_bitmap_t* inverted_bitmap = bitmap_not(bitmap);
  int64_t element = bitmap_get_nth_element_present(inverted_bitmap, n);
  roaring_bitmap_free(inverted_bitmap);
  return element;
}

uint64_t bitmap64_get_nth_element_not_present_slow(const Bitmap64* bitmap, uint64_t n, bool* found) {
  roaring64_bitmap_t* inverted_bitmap = bitmap64_not(bitmap);
  uint64_t element = bitmap64_get_nth_element_present(inverted_bitmap, n, found);
  roaring64_bitmap_free(inverted_bitmap);
  return element;
}

// [TODO] Replace with roaring64_bitmap_overwrite in next CRoaring release 
static void _roaring64_bitmap_overwrite(Bitmap64* dest, const Bitmap64* src) {
  if (!roaring64_bitmap_is_empty(dest)) {
    roaring64_bitmap_clear(dest);
  }

  roaring64_bitmap_or_inplace(dest, src);
}

void bitmap_or(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) return;

  roaring_bitmap_overwrite(r, bitmaps[0]);

  for (size_t i = 1; i < n; i++) {
    roaring_bitmap_lazy_or_inplace(r, (Bitmap*) bitmaps[i], false);
  }

  roaring_bitmap_repair_after_lazy(r);
}

void bitmap64_or(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) return;

  _roaring64_bitmap_overwrite(r, bitmaps[0]);

  for (uint32_t i = 1; i < n; i++) {
    roaring64_bitmap_or_inplace(r, bitmaps[i]);
  }
}

void bitmap_and(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) return;

  roaring_bitmap_overwrite(r, bitmaps[0]);

  for (uint32_t i = 1; i < n; i++) {
    roaring_bitmap_and_inplace(r, bitmaps[i]);
  }
}

void bitmap64_and(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) return;

  _roaring64_bitmap_overwrite(r, bitmaps[0]);

  for (uint32_t i = 1; i < n; i++) {
    roaring64_bitmap_and_inplace(r, bitmaps[i]);
  }
}

void bitmap_xor(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) return;

  roaring_bitmap_overwrite(r, bitmaps[0]);

  for (uint32_t i = 1; i < n; i++) {
    roaring_bitmap_lazy_xor_inplace(r, bitmaps[i]);
  }

  roaring_bitmap_repair_after_lazy(r);
}

void bitmap64_xor(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) return;

  _roaring64_bitmap_overwrite(r, bitmaps[0]);

  for (uint32_t i = 1; i < n; i++) {
    roaring64_bitmap_xor_inplace(r, bitmaps[i]);
  }
}

void bitmap_andor(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) return;
  if (n < 2) return bitmap_and(r, n, bitmaps);

  const Bitmap* x = bitmaps[0];

  roaring_bitmap_overwrite(r, bitmaps[1]);

  for (size_t i = 2; i < n; i++) {
    roaring_bitmap_lazy_or_inplace(r, bitmaps[i], false);
  }

  roaring_bitmap_repair_after_lazy(r);

  roaring_bitmap_and_inplace(r, x);
}

void bitmap64_andor(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) return;
  if (n < 2) return bitmap64_and(r, n, bitmaps);

  const Bitmap64* x = bitmaps[0];

  _roaring64_bitmap_overwrite(r, bitmaps[1]);

  for (size_t i = 2; i < n; i++) {
    roaring64_bitmap_or_inplace(r, bitmaps[i]);
  }

  roaring64_bitmap_and_inplace(r, x);
}

void bitmap_one(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) {
    return roaring_bitmap_clear(r);
  }
  if (n == 1) {
    roaring_bitmap_overwrite(r, bitmaps[0]);
    return;
  }
  // Do simple xor for two bitmaps
  if (n < 2) {
    return bitmap_xor(r, n, bitmaps);
  }

  // Helper bitmap to track bits seen in more than one key
  Bitmap* helper = roaring_bitmap_create();

  // Start with the first bitmap
  roaring_bitmap_overwrite(r, bitmaps[0]);

  for (uint32_t i = 1; i < n; i++) {
    // Track bits that appear in both current result and new bitmap
    // These are bits that now appear in more than one key
    Bitmap* intersection = roaring_bitmap_and(r, bitmaps[i]);
    roaring_bitmap_or_inplace(helper, intersection);
    roaring_bitmap_free(intersection);

    // XOR the new bitmap with current result
    roaring_bitmap_xor_inplace(r, bitmaps[i]);

    // Remove bits that have been seen in more than one key
    roaring_bitmap_andnot_inplace(r, helper);
  }

  roaring_bitmap_free(helper);
}

void bitmap64_one(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) {
    return roaring64_bitmap_clear(r);
  }
  if (n == 1) {
    return _roaring64_bitmap_overwrite(r, bitmaps[0]);
  }
  // Do simple xor for two bitmaps
  if (n < 2) {
    return bitmap64_xor(r, n, bitmaps);
  }

  // Helper bitmap to track bits seen in more than one key
  Bitmap64* helper = roaring64_bitmap_create();

  // Start with the first bitmap
  _roaring64_bitmap_overwrite(r, bitmaps[0]);

  for (uint32_t i = 1; i < n; i++) {
    // Track bits that appear in both current result and new bitmap
    // These are bits that now appear in more than one key
    Bitmap64* intersection = roaring64_bitmap_and(r, bitmaps[i]);
    roaring64_bitmap_or_inplace(helper, intersection);
    roaring64_bitmap_free(intersection);

    // XOR the new bitmap with current result
    roaring64_bitmap_xor_inplace(r, bitmaps[i]);

    // Remove bits that have been seen in more than one key
    roaring64_bitmap_andnot_inplace(r, helper);
  }

  roaring64_bitmap_free(helper);
}

Bitmap* bitmap_not_array(uint32_t unused, const Bitmap** bitmaps) {
  (void) (unused);
  uint32_t last = roaring_bitmap_maximum(bitmaps[0]);
  return roaring_bitmap_flip(bitmaps[0], 0, last + 1);
}

Bitmap64* bitmap64_not_array(uint64_t unused, const Bitmap64** bitmaps) {
  (void) (unused);
  uint64_t last = roaring64_bitmap_maximum(bitmaps[0]);
  return roaring64_bitmap_flip(bitmaps[0], 0, last + 1);
}

Bitmap* bitmap_flip(const Bitmap* bitmap, uint32_t end) {
  return roaring_bitmap_flip(bitmap, 0, end);
}

Bitmap64* bitmap64_flip(const Bitmap64* bitmap, uint64_t end) {
  return roaring64_bitmap_flip(bitmap, 0, end);
}

Bitmap* bitmap_not(const Bitmap* bitmap) {
  const Bitmap* bitmaps[] = { bitmap };
  return bitmap_not_array(1, bitmaps);
}

Bitmap64* bitmap64_not(const Bitmap64* bitmap) {
  const Bitmap64* bitmaps[] = { bitmap };
  return bitmap64_not_array(1, bitmaps);
}

Bitmap* bitmap_from_int_array(size_t n, const uint32_t* array) {
  return roaring_bitmap_of_ptr(n, array);
}

Bitmap64* bitmap64_from_int_array(size_t n, const uint64_t* array) {
  return roaring64_bitmap_of_ptr(n, array);
}

uint32_t* bitmap_get_int_array(const Bitmap* bitmap, size_t* n) {
  *n = roaring_bitmap_get_cardinality(bitmap);
  uint32_t* ans = rm_malloc(sizeof(*ans) * (*n));
  roaring_bitmap_to_uint32_array(bitmap, ans);
  return ans;
}

uint64_t* bitmap64_get_int_array(const Bitmap64* bitmap, uint64_t* n) {
  *n = roaring64_bitmap_get_cardinality(bitmap);
  uint64_t* ans = rm_malloc(sizeof(*ans) * (*n));
  roaring64_bitmap_to_uint64_array(bitmap, ans);
  return ans;
}

uint32_t* bitmap_range_int_array(const Bitmap* bitmap, size_t offset, size_t n) {
  uint32_t* ans = rm_calloc(n, sizeof(*ans));
  roaring_bitmap_range_uint32_array(bitmap, offset, n, ans);
  return ans;
}

uint64_t* bitmap64_range_int_array(const Bitmap64* bitmap, uint64_t offset, uint64_t n) {
  uint64_t* ans = rm_calloc(n, sizeof(*ans));

  // [TODO] replace with roaring64_bitmap_range_uint64_array in next version of CRoaring
  for (uint64_t i = 0; i < n; i++) {
    if (!roaring64_bitmap_select(bitmap, offset + i, &ans[i])) {
      break;
    }
  }

  return ans;
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

Bitmap64* bitmap64_from_bit_array(size_t size, const char* array) {
  Bitmap64* bitmap = bitmap64_alloc();
  for (size_t i = 0; i < size; i++) {
    if (array[i] == '1') {
      roaring64_bitmap_add(bitmap, (uint64_t) i);
    }
  }
  return bitmap;
}

char* bitmap_get_bit_array(const Bitmap* bitmap, size_t* size) {
  *size = roaring_bitmap_maximum(bitmap) + 1;
  char* ans = rm_malloc(*size + 1);
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

char* bitmap64_get_bit_array(const Bitmap64* bitmap, uint64_t* size) {
  *size = roaring64_bitmap_maximum(bitmap) + 1;
  char* ans = rm_malloc(*size + 1);
  memset(ans, '0', *size);
  ans[*size] = '\0';

  roaring64_iterator_t* iterator = roaring64_iterator_create(bitmap);
  while (roaring64_iterator_has_value(iterator)) {
    ans[roaring64_iterator_value(iterator)] = '1';
    roaring64_iterator_advance(iterator);
  }
  roaring64_iterator_free(iterator);

  return ans;
}

void bitmap_free_bit_array(char* array) {
  rm_free(array);
}

Bitmap* bitmap_from_range(uint64_t from, uint64_t to) {
  // allocate empty bitmap when invalid range
  if (from == to) {
    return bitmap_alloc();
  }

  return roaring_bitmap_from_range(from, to, 1);
}

Bitmap64* bitmap64_from_range(uint64_t from, uint64_t to) {
  // allocate empty bitmap when invalid range
  if (from == to) {
    return bitmap64_alloc();
  }

  return roaring64_bitmap_from_range(from, to, 1);
}

bool bitmap_is_empty(const Bitmap* bitmap) {
  return roaring_bitmap_is_empty(bitmap);
}

bool bitmap64_is_empty(const Bitmap64* bitmap) {
  return roaring64_bitmap_is_empty(bitmap);
}

uint32_t bitmap_min(const Bitmap* bitmap) {
  return roaring_bitmap_minimum(bitmap);
}

uint64_t bitmap64_min(const Bitmap64* bitmap) {
  return roaring64_bitmap_minimum(bitmap);
}

uint32_t bitmap_max(const Bitmap* bitmap) {
  return roaring_bitmap_maximum(bitmap);
}

uint64_t bitmap64_max(const Bitmap64* bitmap) {
  return roaring64_bitmap_maximum(bitmap);
}

void bitmap_optimize(Bitmap* bitmap, int shrink_to_fit) {
  roaring_bitmap_run_optimize(bitmap);
  if (shrink_to_fit) {
    roaring_bitmap_shrink_to_fit(bitmap);
  }
}

void bitmap64_optimize(Bitmap64* bitmap, int shrink_to_fit) {
  roaring64_bitmap_run_optimize(bitmap);
  if (shrink_to_fit) {
    roaring64_bitmap_shrink_to_fit(bitmap);
  }
}

void bitmap_statistics(const Bitmap* bitmap, Bitmap_statistics* stat) {
  roaring_bitmap_statistics(bitmap, stat);
}

void bitmap64_statistics(const Bitmap64* bitmap, Bitmap64_statistics* stat) {
  roaring64_bitmap_statistics(bitmap, stat);
}

size_t uint64_to_string(uint64_t value, char* buffer) {
  if (value == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return 1;
  }

  char* ptr = buffer;
  char* start = buffer;

  // Extract digits in reverse order
  while (value > 0) {
    *ptr++ = '0' + (value % 10);
    value /= 10;
  }

  size_t len = ptr - buffer;
  *ptr = '\0';

  // Reverse the string in-place
  ptr--;
  while (start < ptr) {
    char temp = *start;
    *start++ = *ptr;
    *ptr-- = temp;
  }

  return len;
}
