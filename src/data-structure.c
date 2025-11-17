#include <math.h>
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

bool bitmap_setbit(Bitmap* bitmap, uint32_t offset, bool value) {
  if (!value) {
    return roaring_bitmap_remove_checked(bitmap, offset);
  } else {
    return !roaring_bitmap_add_checked(bitmap, offset);
  }
}

bool bitmap64_setbit(Bitmap64* bitmap, uint64_t offset, bool value) {
  if (!value) {
    return roaring64_bitmap_remove_checked(bitmap, offset);
  } else {
    return !roaring64_bitmap_add_checked(bitmap, offset);
  }
}

bool bitmap_getbit(const Bitmap* bitmap, uint32_t offset) {
  return roaring_bitmap_contains(bitmap, offset);
}

bool bitmap64_getbit(const Bitmap64* bitmap, uint64_t offset) {
  return roaring64_bitmap_contains(bitmap, offset);
}

bool* bitmap_getbits(const Bitmap* bitmap, size_t n_offsets, const uint32_t* offsets) {
  if (bitmap == NULL) return NULL;

  roaring_bulk_context_t context = CROARING_ZERO_INITIALIZER;

  bool* results = rm_malloc(sizeof(bool) * n_offsets);

  for (size_t i = 0; i < n_offsets; i++) {
    results[i] = roaring_bitmap_contains_bulk(bitmap, &context, offsets[i]);
  }

  return results;
}

bool* bitmap64_getbits(const Bitmap64* bitmap, size_t n_offsets, const uint64_t* offsets) {
  if (bitmap == NULL) return NULL;

  roaring64_bulk_context_t context = CROARING_ZERO_INITIALIZER;

  bool* results = rm_malloc(sizeof(bool) * n_offsets);

  for (size_t i = 0; i < n_offsets; i++) {
    results[i] = roaring64_bitmap_contains_bulk(bitmap, &context, offsets[i]);
  }

  return results;
}

bool bitmap_clearbits(Bitmap* bitmap, size_t n_offsets, const uint32_t* offsets) {
  if (bitmap == NULL) return false;
  if (offsets == NULL) return true;

  roaring_bitmap_remove_many(bitmap, n_offsets, offsets);

  return true;
}

bool bitmap64_clearbits(Bitmap64* bitmap, size_t n_offsets, const uint64_t* offsets) {
  if (bitmap == NULL) return false;
  if (offsets == NULL) return true;

  roaring64_bitmap_remove_many(bitmap, n_offsets, offsets);

  return true;
}

size_t bitmap_clearbits_count(Bitmap* bitmap, size_t n_offsets, const uint32_t* offsets) {
  if (bitmap == NULL || offsets == NULL) return 0;

  size_t count = 0;

  for (size_t i = 0; i < n_offsets; i++) {
    if (roaring_bitmap_remove_checked(bitmap, offsets[i])) {
      count++;
    }
  }

  return count++;
}

size_t bitmap64_clearbits_count(Bitmap64* bitmap, size_t n_offsets, const uint64_t* offsets) {
  if (bitmap == NULL || offsets == NULL) return 0;

  size_t count = 0;

  for (size_t i = 0; i < n_offsets; i++) {
    if (roaring64_bitmap_remove_checked(bitmap, offsets[i])) {
      count++;
    }
  }

  return count++;
}

bool bitmap_intersect(const Bitmap* b1, const Bitmap* b2, uint32_t mode) {
  if (b1 == NULL || b2 == NULL) return false;

  switch (mode) {
  case BITMAP_INTERSECT_MODE_NONE:
    return roaring_bitmap_intersect(b1, b2);

  case BITMAP_INTERSECT_MODE_ALL:
    return roaring_bitmap_is_subset(b2, b1);

  case BITMAP_INTERSECT_MODE_ALL_STRICT:
    return roaring_bitmap_is_strict_subset(b2, b1);

  case BITMAP_INTERSECT_MODE_EQ:
    return roaring_bitmap_equals(b1, b2);

  default:
    return false;
  }
}

bool bitmap64_intersect(const Bitmap64* b1, const Bitmap64* b2, uint32_t mode) {
  if (b1 == NULL || b2 == NULL) return false;

  switch (mode) {
  case BITMAP_INTERSECT_MODE_NONE:
    return roaring64_bitmap_intersect(b1, b2);

  case BITMAP_INTERSECT_MODE_ALL:
    return roaring64_bitmap_is_subset(b2, b1);

  case BITMAP_INTERSECT_MODE_ALL_STRICT:
    return roaring64_bitmap_is_strict_subset(b2, b1);

  case BITMAP_INTERSECT_MODE_EQ:
    return roaring64_bitmap_equals(b1, b2);

  default:
    return false;
  }
}

double bitmap_jaccard(const Bitmap* b1, const Bitmap* b2) {
  if (b1 == NULL || b2 == NULL) return 0;
  if (b1 == b2) return 1;

  double res = roaring_bitmap_jaccard_index(b1, b2);

  // both bitmaps are empty
  if (isnan(res)) res = -1;

  return res;
}

double bitmap64_jaccard(const Bitmap64* b1, const Bitmap64* b2) {
  if (b1 == NULL || b2 == NULL) return 0;
  if (b1 == b2) return 1;

  double res = roaring64_bitmap_jaccard_index(b1, b2);

  // both bitmaps are empty
  if (isnan(res)) res = -1;

  return res;
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
  if (n == 0) {
    return roaring_bitmap_clear(r);
  }
  if (n == 1) {
    roaring_bitmap_overwrite(r, bitmaps[0]);
    return;
  }

  // Only overwrite if r != bitmaps[0], otherwise r would be cleared and then copied from itself (resulting in empty)
  if (r != bitmaps[0]) {
    roaring_bitmap_overwrite(r, bitmaps[0]);
  }

  for (size_t i = 1; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Use non-inplace operation to avoid memcpy overlap in shared containers
      Bitmap* temp = roaring_bitmap_lazy_or(r, bitmaps[i], false);
      roaring_bitmap_overwrite(r, temp);
      roaring_bitmap_free(temp);
    }
  }

  roaring_bitmap_repair_after_lazy(r);
}

void bitmap64_or(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) {
    return roaring64_bitmap_clear(r);
  }
  if (n == 1) {
    _roaring64_bitmap_overwrite(r, bitmaps[0]);
    return;
  }

  // Only overwrite if r != bitmaps[0], otherwise r would be cleared and then copied from itself (resulting in empty)
  if (r != bitmaps[0]) {
  _roaring64_bitmap_overwrite(r, bitmaps[0]);
  }

  for (uint32_t i = 1; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Use non-inplace operation to avoid memcpy overlap in shared containers
      Bitmap64* temp = roaring64_bitmap_or(r, bitmaps[i]);
      _roaring64_bitmap_overwrite(r, temp);
      roaring64_bitmap_free(temp);
    }
  }
}

void bitmap_and(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) {
    return roaring_bitmap_clear(r);
  }
  if (n == 1) {
    roaring_bitmap_overwrite(r, bitmaps[0]);
    return;
  }

  // Only overwrite if r != bitmaps[0], otherwise r would be cleared and then copied from itself (resulting in empty)
  if (r != bitmaps[0]) {
    roaring_bitmap_overwrite(r, bitmaps[0]);
  }

  for (uint32_t i = 1; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Use non-inplace operation to avoid memcpy overlap in shared containers
      Bitmap* temp = roaring_bitmap_and(r, bitmaps[i]);
      roaring_bitmap_overwrite(r, temp);
      roaring_bitmap_free(temp);
    }
  }
}

void bitmap64_and(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) {
    return roaring64_bitmap_clear(r);
  }
  if (n == 1) {
    _roaring64_bitmap_overwrite(r, bitmaps[0]);
    return;
  }

  // Only overwrite if r != bitmaps[0], otherwise r would be cleared and then copied from itself (resulting in empty)
  if (r != bitmaps[0]) {
  _roaring64_bitmap_overwrite(r, bitmaps[0]);
  }

  for (uint32_t i = 1; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Use non-inplace operation to avoid memcpy overlap in shared containers
      Bitmap64* temp = roaring64_bitmap_and(r, bitmaps[i]);
      _roaring64_bitmap_overwrite(r, temp);
      roaring64_bitmap_free(temp);
    }
  }
}

void bitmap_xor(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) {
    return roaring_bitmap_clear(r);
  }
  if (n == 1) {
    roaring_bitmap_overwrite(r, bitmaps[0]);
    return;
  }

  // Only overwrite if r != bitmaps[0], otherwise r would be cleared and then copied from itself (resulting in empty)
  if (r != bitmaps[0]) {
  roaring_bitmap_overwrite(r, bitmaps[0]);
  }

  for (uint32_t i = 1; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Use non-inplace operation to avoid memcpy overlap in shared containers
      Bitmap* temp = roaring_bitmap_lazy_xor(r, bitmaps[i]);
      roaring_bitmap_overwrite(r, temp);
      roaring_bitmap_free(temp);
    }
  }

  roaring_bitmap_repair_after_lazy(r);
}

void bitmap64_xor(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) {
    return roaring64_bitmap_clear(r);
  }
  if (n == 1) {
    _roaring64_bitmap_overwrite(r, bitmaps[0]);
    return;
  }

  // Only overwrite if r != bitmaps[0], otherwise r would be cleared and then copied from itself (resulting in empty)
  if (r != bitmaps[0]) {
  _roaring64_bitmap_overwrite(r, bitmaps[0]);
  }

  for (uint32_t i = 1; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Use non-inplace operation to avoid memcpy overlap in shared containers
      Bitmap64* temp = roaring64_bitmap_xor(r, bitmaps[i]);
      _roaring64_bitmap_overwrite(r, temp);
      roaring64_bitmap_free(temp);
    }
  }
}

void bitmap_andor(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) {
    return roaring_bitmap_clear(r);
  }
  if (n == 1) {
    roaring_bitmap_overwrite(r, bitmaps[0]);
    return;
  }
  if (n < 2) {
    return bitmap_and(r, n, bitmaps);
  }

  // If r == bitmaps[0], we need to make a copy because we'll overwrite r
  Bitmap* x_copy = NULL;
  const Bitmap* x = bitmaps[0];
  if (r == bitmaps[0]) {
    x_copy = roaring_bitmap_copy(bitmaps[0]);
    x = x_copy;
  }

  // Only overwrite if r != bitmaps[1], otherwise r would be cleared and then copied from itself
  if (r != bitmaps[1]) {
    roaring_bitmap_overwrite(r, bitmaps[1]);
  }

  for (size_t i = 2; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Use non-inplace operation to avoid memcpy overlap in shared containers
      Bitmap* temp = roaring_bitmap_lazy_or(r, bitmaps[i], false);
      roaring_bitmap_overwrite(r, temp);
      roaring_bitmap_free(temp);
    }
  }

  roaring_bitmap_repair_after_lazy(r);

  // Now AND with x (either bitmaps[0] or our copy if r was bitmaps[0])
  Bitmap* temp = roaring_bitmap_and(r, x);
  roaring_bitmap_overwrite(r, temp);
  roaring_bitmap_free(temp);

  // Clean up copy if we made one
  if (x_copy) {
    roaring_bitmap_free(x_copy);
  }
}

void bitmap_andnot(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) {
    return roaring_bitmap_clear(r);
  }
  if (n == 1) {
    roaring_bitmap_overwrite(r, bitmaps[0]);
    return;
  }
  if (n == 2) {
    roaring_bitmap_overwrite(r, bitmaps[0]);
    if (bitmaps[1] != r) {  // Skip if same pointer to avoid memcpy overlap
      Bitmap* temp = roaring_bitmap_andnot(r, bitmaps[1]);
      roaring_bitmap_overwrite(r, temp);
      roaring_bitmap_free(temp);
    }
    return;
  }

  const Bitmap* x = bitmaps[0];

  roaring_bitmap_overwrite(r, x);

  for (uint32_t i = 1; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Use non-inplace operation to avoid memcpy overlap in shared containers
      Bitmap* temp = roaring_bitmap_andnot(r, bitmaps[i]);
      roaring_bitmap_overwrite(r, temp);
      roaring_bitmap_free(temp);
    }
  }
}

void bitmap64_andnot(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) {
    return roaring64_bitmap_clear(r);
  }
  if (n == 1) {
    _roaring64_bitmap_overwrite(r, bitmaps[0]);
    return;
  }
  if (n == 2) {
    _roaring64_bitmap_overwrite(r, bitmaps[0]);
    if (bitmaps[1] != r) {  // Skip if same pointer to avoid memcpy overlap
      Bitmap64* temp = roaring64_bitmap_andnot(r, bitmaps[1]);
      _roaring64_bitmap_overwrite(r, temp);
      roaring64_bitmap_free(temp);
    }
    return;
  }

  const Bitmap64* x = bitmaps[0];

  _roaring64_bitmap_overwrite(r, x);

  for (uint32_t i = 1; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      Bitmap64* temp = roaring64_bitmap_andnot(r, bitmaps[i]);
      _roaring64_bitmap_overwrite(r, temp);
      roaring64_bitmap_free(temp);
    }
  }
}

void bitmap_ornot(Bitmap* r, uint32_t n, const Bitmap** bitmaps) {
  if (n == 0) {
    return roaring_bitmap_clear(r);
  }
  if (n == 1) {
    return roaring_bitmap_clear(r);
  }
  if (n == 2) {
    roaring_bitmap_overwrite(r, bitmaps[1]);
    if (bitmaps[0] != r) {  // Skip if same pointer to avoid memcpy overlap
      Bitmap* temp = roaring_bitmap_andnot(r, bitmaps[0]);
      roaring_bitmap_overwrite(r, temp);
      roaring_bitmap_free(temp);
    }
    return;
  }

  const Bitmap* x = bitmaps[0];

  roaring_bitmap_overwrite(r, bitmaps[1]);

  for (uint32_t i = 2; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Use non-inplace operation to avoid memcpy overlap in shared containers
      Bitmap* temp = roaring_bitmap_or(r, bitmaps[i]);
      roaring_bitmap_overwrite(r, temp);
      roaring_bitmap_free(temp);
    }
  }

  if (x != r) {  // Skip if same pointer to avoid memcpy overlap
    Bitmap* temp = roaring_bitmap_andnot(r, x);
    roaring_bitmap_overwrite(r, temp);
    roaring_bitmap_free(temp);
  }
}

void bitmap64_ornot(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) {
    return roaring64_bitmap_clear(r);
  }
  if (n == 1) {
    return roaring64_bitmap_clear(r);
  }
  if (n == 2) {
    _roaring64_bitmap_overwrite(r, bitmaps[1]);
    if (bitmaps[0] != r) {  // Skip if same pointer to avoid memcpy overlap
      Bitmap64* temp = roaring64_bitmap_andnot(r, bitmaps[0]);
      _roaring64_bitmap_overwrite(r, temp);
      roaring64_bitmap_free(temp);
    }
    return;
  }

  const Bitmap64* x = bitmaps[0];

  _roaring64_bitmap_overwrite(r, bitmaps[1]);

  for (uint32_t i = 2; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      Bitmap64* temp = roaring64_bitmap_or(r, bitmaps[i]);
      _roaring64_bitmap_overwrite(r, temp);
      roaring64_bitmap_free(temp);
    }
  }

  if (x != r) {  // Skip if same pointer to avoid memcpy overlap
    Bitmap64* temp = roaring64_bitmap_andnot(r, x);
    _roaring64_bitmap_overwrite(r, temp);
    roaring64_bitmap_free(temp);
  }
}

void bitmap64_andor(Bitmap64* r, uint32_t n, const Bitmap64** bitmaps) {
  if (n == 0) {
    return roaring64_bitmap_clear(r);
  }
  if (n == 1) {
    _roaring64_bitmap_overwrite(r, bitmaps[0]);
    return;
  }
  if (n < 2) {
    return bitmap64_and(r, n, bitmaps);
  }

  // If r == bitmaps[0], we need to make a copy because we'll overwrite r
  Bitmap64* x_copy = NULL;
  const Bitmap64* x = bitmaps[0];
  if (r == bitmaps[0]) {
    x_copy = roaring64_bitmap_copy(bitmaps[0]);
    x = x_copy;
  }

  // Only overwrite if r != bitmaps[1], otherwise r would be cleared and then copied from itself
  if (r != bitmaps[1]) {
    _roaring64_bitmap_overwrite(r, bitmaps[1]);
  }

  for (size_t i = 2; i < n; i++) {
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      Bitmap64* temp = roaring64_bitmap_or(r, bitmaps[i]);
      _roaring64_bitmap_overwrite(r, temp);
      roaring64_bitmap_free(temp);
    }
  }

  // Now AND with x (either bitmaps[0] or our copy if r was bitmaps[0])
  Bitmap64* temp = roaring64_bitmap_and(r, x);
  _roaring64_bitmap_overwrite(r, temp);
  roaring64_bitmap_free(temp);

  // Clean up copy if we made one
  if (x_copy) {
    roaring64_bitmap_free(x_copy);
  }
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
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Track bits that appear in both current result and new bitmap
      // These are bits that now appear in more than one key
      Bitmap* intersection = roaring_bitmap_and(r, bitmaps[i]);
      Bitmap* helper_temp = roaring_bitmap_or(helper, intersection);
      roaring_bitmap_overwrite(helper, helper_temp);
      roaring_bitmap_free(helper_temp);
      roaring_bitmap_free(intersection);

      // XOR the new bitmap with current result
      Bitmap* xor_temp = roaring_bitmap_xor(r, bitmaps[i]);
      roaring_bitmap_overwrite(r, xor_temp);
      roaring_bitmap_free(xor_temp);

      // Remove bits that have been seen in more than one key
      Bitmap* andnot_temp = roaring_bitmap_andnot(r, helper);
      roaring_bitmap_overwrite(r, andnot_temp);
      roaring_bitmap_free(andnot_temp);
    }
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
    if (bitmaps[i] != r) {  // Skip if same pointer to avoid memcpy overlap
      // Track bits that appear in both current result and new bitmap
      // These are bits that now appear in more than one key
      Bitmap64* intersection = roaring64_bitmap_and(r, bitmaps[i]);
      Bitmap64* helper_temp = roaring64_bitmap_or(helper, intersection);
      _roaring64_bitmap_overwrite(helper, helper_temp);
      roaring64_bitmap_free(helper_temp);
      roaring64_bitmap_free(intersection);

      // XOR the new bitmap with current result
      Bitmap64* xor_temp = roaring64_bitmap_xor(r, bitmaps[i]);
      _roaring64_bitmap_overwrite(r, xor_temp);
      roaring64_bitmap_free(xor_temp);

      // Remove bits that have been seen in more than one key
      Bitmap64* andnot_temp = roaring64_bitmap_andnot(r, helper);
      _roaring64_bitmap_overwrite(r, andnot_temp);
      roaring64_bitmap_free(andnot_temp);
    }
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

uint32_t* bitmap_range_int_array(const Bitmap* bitmap, size_t start_offset, size_t end_offset, size_t* result_count) {
  if (result_count) *result_count = 0;

  // Validate input parameters
  if (bitmap == NULL) {
    return NULL;
  }

  // Validate range parameters
  if (start_offset > end_offset) {
    return NULL;
  }

  uint32_t range_size = (end_offset - start_offset) + 1;

  // Check for overflow
  if (range_size < (end_offset - start_offset)) {
    return NULL;
  }

  uint32_t* ans = rm_calloc_try(range_size, sizeof(uint32_t));

  roaring_bitmap_range_uint32_array(bitmap, start_offset, range_size, ans);

  // Determine actual count of set bits by finding the first zero entry
  // Special case: when querying from offset 0, verify that bit 0 is actually set
  // since zero values in the array are ambiguous (could be unset bit or actual zero offset)
  if (start_offset == 0 && ans[0] == 0) {
    if (!roaring_bitmap_contains(bitmap, 0)) {
      return ans;
    }
  } else if (ans[0] == 0) {
    // No set bits found in range (first element is zero and we're not starting from offset 0)
    return ans;
  }

  uint32_t actual_count = 1;

  // Count consecutive non-zero entries to determine actual result size
  while (actual_count < range_size) {
    if (ans[actual_count] == 0) break;
    actual_count++;
  }

  if (result_count) *result_count = actual_count;

  return ans;
}

uint64_t* bitmap64_range_int_array(const Bitmap64* bitmap, uint64_t start_offset, uint64_t end_offset, uint64_t* result_count) {
  if (result_count) *result_count = 0;

  // Validate input parameters
  if (bitmap == NULL) {
    return NULL;
  }

  // Validate range parameters
  if (start_offset > end_offset) {
    return NULL;
  }

  uint64_t range_size = (end_offset - start_offset) + 1;

  // Check for overflow
  if (range_size < (end_offset - start_offset)) {
    return NULL;
  }

  uint64_t* ans = rm_calloc_try(range_size, sizeof(uint64_t));

  if (ans == NULL) {
    return NULL;
  }

  uint64_t i = 0;

  // [TODO] replace with roaring64_bitmap_range_uint64_array in next version of CRoaring
  while (i < range_size) {
    if (!roaring64_bitmap_select(bitmap, start_offset + i, &ans[i])) {
      break;
    }

    i++;
  }

  if (result_count) *result_count = i;

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

bool bitmap_optimize(Bitmap* bitmap, int shrink_to_fit) {
  bool was_modified = roaring_bitmap_run_optimize(bitmap);
  if (shrink_to_fit && was_modified) {
    roaring_bitmap_shrink_to_fit(bitmap);
  }
  return was_modified;
}

bool bitmap64_optimize(Bitmap64* bitmap, int shrink_to_fit) {
  bool was_modified = roaring64_bitmap_run_optimize(bitmap);
  if (shrink_to_fit && was_modified) {
    roaring64_bitmap_shrink_to_fit(bitmap);
  }
  return was_modified;
}

void bitmap_statistics(const Bitmap* bitmap, Bitmap_statistics* stat) {
  roaring_bitmap_statistics(bitmap, stat);
}

void bitmap64_statistics(const Bitmap64* bitmap, Bitmap64_statistics* stat) {
  roaring64_bitmap_statistics(bitmap, stat);
}

static inline int bitmap_stat_json(const Bitmap* bitmap, char** result) {
  roaring_statistics_t stats;
  roaring_bitmap_statistics(bitmap, &stats);

  return asprintf(result,
    "{"
    "\"type\":\"bitmap\","
    "\"cardinality\":\"%llu\","
    "\"number_of_containers\":\"%u\","
    "\"max_value\":\"%u\","
    "\"min_value\":\"%u\","
    "\"array_container\":{"
    "\"number_of_containers\":\"%u\","
    "\"container_cardinality\":\"%u\","
    "\"container_allocated_bytes\":\"%u\"},"
    "\"bitset_container\":{"
    "\"number_of_containers\":\"%u\","
    "\"container_cardinality\":\"%u\","
    "\"container_allocated_bytes\":\"%u\"},"
    "\"run_container\":{"
    "\"number_of_containers\":\"%u\","
    "\"container_cardinality\":\"%u\","
    "\"container_allocated_bytes\":\"%u\"}"
    "}",
    roaring_bitmap_get_cardinality(bitmap),
    stats.n_containers,
    roaring_bitmap_maximum(bitmap),
    roaring_bitmap_minimum(bitmap),
    stats.n_array_containers,
    stats.n_values_array_containers,
    stats.n_bytes_array_containers,
    stats.n_bitset_containers,
    stats.n_values_bitset_containers,
    stats.n_bytes_bitset_containers,
    stats.n_run_containers,
    stats.n_values_run_containers,
    stats.n_bytes_run_containers
  );
}

static inline int bitmap_stat_plain(const Bitmap* bitmap, char** result) {
  roaring_statistics_t stats;
  roaring_bitmap_statistics(bitmap, &stats);

  return asprintf(result,
    "type: bitmap\n"
    "cardinality: %llu\n"
    "number of containers: %u\n"
    "max value: %u\n"
    "min value: %u\n"
    "number of array containers: %u\n"
    "\tarray container values: %u\n"
    "\tarray container bytes: %u\n"
    "bitset  containers: %u\n"
    "\tbitset  container values: %u\n"
    "\tbitset  container bytes: %u\n"
    "run containers: %u\n"
    "\trun container values: %u\n"
    "\trun container bytes: %u\n"
    ,
    roaring_bitmap_get_cardinality(bitmap),
    stats.n_containers,
    roaring_bitmap_maximum(bitmap),
    roaring_bitmap_minimum(bitmap),
    stats.n_array_containers,
    stats.n_values_array_containers,
    stats.n_bytes_array_containers,
    stats.n_bitset_containers,
    stats.n_values_bitset_containers,
    stats.n_bytes_bitset_containers,
    stats.n_run_containers,
    stats.n_values_run_containers,
    stats.n_bytes_run_containers
  );
}

/**
 * Computes string statistics of roaring bitmap
 * Returns `NULL` when failed.
 * You is responsible for calling `free()`
 */
char* bitmap_statistics_str(const Bitmap* bitmap, int format, int* size) {
  if (!bitmap) {
    return NULL;
  }

  char* result = NULL;
  int result_size = -1;

  switch (format) {
  case BITMAP_STATISTICS_FORMAT_JSON:
    result_size = bitmap_stat_json(bitmap, &result);
    break;
  case BITMAP_STATISTICS_FORMAT_PLAIN_TEXT:
    result_size = bitmap_stat_plain(bitmap, &result);
    break;
  default:
    return NULL;
  }

  if (result_size == -1) {
    if (result) {
      rm_free(result);
      result = NULL;
    }
  }

  if (size) {
    *size = result_size;
  }

  return result;
}

static inline int bitmap64_stat_json(const Bitmap64* bitmap, char** result) {
  roaring64_statistics_t stats;
  roaring64_bitmap_statistics(bitmap, &stats);

  return asprintf(result,
    "{"
    "\"type\":\"bitmap64\","
    "\"cardinality\":\"%llu\","
    "\"number_of_containers\":\"%llu\","
    "\"max_value\":\"%llu\","
    "\"min_value\":\"%llu\","
    "\"array_container\":{"
    "\"number_of_containers\":\"%llu\","
    "\"container_cardinality\":\"%llu\","
    "\"container_allocated_bytes\":\"%llu\"},"
    "\"bitset_container\":{"
    "\"number_of_containers\":\"%llu\","
    "\"container_cardinality\":\"%llu\","
    "\"container_allocated_bytes\":\"%llu\"},"
    "\"run_container\":{"
    "\"number_of_containers\":\"%llu\","
    "\"container_cardinality\":\"%llu\","
    "\"container_allocated_bytes\":\"%llu\"}"
    "}",
    roaring64_bitmap_get_cardinality(bitmap),
    stats.n_containers,
    roaring64_bitmap_maximum(bitmap),
    roaring64_bitmap_minimum(bitmap),
    stats.n_array_containers,
    stats.n_values_array_containers,
    stats.n_bytes_array_containers,
    stats.n_bitset_containers,
    stats.n_values_bitset_containers,
    stats.n_bytes_bitset_containers,
    stats.n_run_containers,
    stats.n_values_run_containers,
    stats.n_bytes_run_containers
  );
}

static inline int bitmap64_stat_plain(const Bitmap64* bitmap, char** result) {
  roaring64_statistics_t stats;
  roaring64_bitmap_statistics(bitmap, &stats);

  return asprintf(result,
    "type: bitmap64\n"
    "cardinality: %llu\n"
    "number of containers: %llu\n"
    "max value: %llu\n"
    "min value: %llu\n"
    "number of array containers: %llu\n"
    "\tarray container values: %llu\n"
    "\tarray container bytes: %llu\n"
    "bitset  containers: %llu\n"
    "\tbitset  container values: %llu\n"
    "\tbitset  container bytes: %llu\n"
    "run containers: %llu\n"
    "\trun container values: %llu\n"
    "\trun container bytes: %llu\n"
    ,
    roaring64_bitmap_get_cardinality(bitmap),
    stats.n_containers,
    roaring64_bitmap_maximum(bitmap),
    roaring64_bitmap_minimum(bitmap),
    stats.n_array_containers,
    stats.n_values_array_containers,
    stats.n_bytes_array_containers,
    stats.n_bitset_containers,
    stats.n_values_bitset_containers,
    stats.n_bytes_bitset_containers,
    stats.n_run_containers,
    stats.n_values_run_containers,
    stats.n_bytes_run_containers
  );
}

/**
 * Computes string statistics of roaring bitmap64
 * Returns `NULL` when failed.
 * You is responsible for calling `free()`
 */
char* bitmap64_statistics_str(const Bitmap64* bitmap, int format, int* size) {
  if (!bitmap) {
    return NULL;
  }

  char* result = NULL;
  int result_size = -1;

  switch (format) {
  case BITMAP_STATISTICS_FORMAT_JSON:
    result_size = bitmap64_stat_json(bitmap, &result);
    break;
  case BITMAP_STATISTICS_FORMAT_PLAIN_TEXT:
    result_size = bitmap64_stat_plain(bitmap, &result);
    break;
  default:
    return NULL;
  }

  if (result_size == -1) {
    if (result) {
      rm_free(result);
      result = NULL;
    }
  }

  if (size) {
    *size = result_size;
  }

  return result;
}
