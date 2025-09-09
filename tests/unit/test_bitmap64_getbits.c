#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_getbits() {
  DESCRIBE("bitmap64_getbits")
  {
    IT("Should return correct bits for empty bitmap")
    {
      // Create empty bitmap
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t offsets[] = { 1, 5, 10, 100 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 0, 0, 0, 0 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should return correct bits for bitmap with single element")
    {
      // Create bitmap with single element
      Bitmap64* bitmap = roaring64_bitmap_from(5);

      uint64_t offsets[] = { 1, 5, 10 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 0, 1, 0 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should return correct bits for bitmap with multiple elements")
    {
      // Create bitmap with multiple elements
      Bitmap64* bitmap = roaring64_bitmap_from(1, 5, 100, 1000);

      uint64_t offsets[] = { 1, 2, 5, 50, 100, 500, 1000 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 1, 0, 1, 0, 1, 0, 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle single offset query")
    {
      // Create bitmap
      Bitmap64* bitmap = roaring64_bitmap_from(42);

      uint64_t offsets[] = { 42 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle zero offsets")
    {
      // Create bitmap
      Bitmap64* bitmap = roaring64_bitmap_from(1);

      uint64_t* offsets = NULL;
      size_t n_offsets = 0;

      bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
      bool expected[] = {};

      // Should return valid pointer but with no meaningful data
      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle large offsets correctly")
    {
      // Create bitmap with large values
      Bitmap64* bitmap = roaring64_bitmap_from(UINT64_MAX - 1, UINT64_MAX);

      uint64_t offsets[] = { UINT64_MAX - 2, UINT64_MAX - 1, UINT64_MAX };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 0, 1, 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle duplicate offsets in query")
    {
      // Create bitmap
      Bitmap64* bitmap = roaring64_bitmap_from(10);

      uint64_t offsets[] = { 10, 10, 20, 10 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 1, 1, 0, 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle offsets in non-ascending order")
    {
      // Create bitmap
      Bitmap64* bitmap = roaring64_bitmap_from(1, 5, 10);

      uint64_t offsets[] = { 10, 1, 20, 5 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 1, 1, 0, 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should work with dense bitmap")
    {
      // Create dense bitmap (range of consecutive values)
      Bitmap64* bitmap = roaring64_bitmap_create();
      for (uint64_t i = 100; i < 200; i++) {
        roaring64_bitmap_add(bitmap, i);
      }

      uint64_t offsets[] = { 99, 100, 150, 199, 200 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap64_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 0, 1, 1, 1, 0 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle NULL bitmap gracefully")
    {
      uint64_t offsets[] = { 1, 2, 3 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap64_getbits(NULL, n_offsets, offsets);
      ASSERT_NULL(results);
    }
  }
}
