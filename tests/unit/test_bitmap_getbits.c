#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_getbits() {
  DESCRIBE("bitmap_getbits")
  {
    IT("Should return correct bits for empty bitmap")
    {
      // Create empty bitmap
      Bitmap* bitmap = roaring_bitmap_create();
      uint32_t offsets[] = { 1, 5, 10, 100 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 0, 0, 0, 0 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring_bitmap_free(bitmap);
    }

    IT("Should return correct bits for bitmap with single element")
    {
      // Create bitmap with single element
      Bitmap* bitmap = roaring_bitmap_from(5);

      uint32_t offsets[] = { 1, 5, 10 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 0, 1, 0 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring_bitmap_free(bitmap);
    }

    IT("Should return correct bits for bitmap with multiple elements")
    {
      // Create bitmap with multiple elements
      Bitmap* bitmap = roaring_bitmap_from(1, 5, 100, 1000);

      uint32_t offsets[] = { 1, 2, 5, 50, 100, 500, 1000 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 1, 0, 1, 0, 1, 0, 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring_bitmap_free(bitmap);
    }

    IT("Should handle single offset query")
    {
      // Create bitmap
      Bitmap* bitmap = roaring_bitmap_from(42);

      uint32_t offsets[] = { 42 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring_bitmap_free(bitmap);
    }

    IT("Should handle zero offsets")
    {
      // Create bitmap
      Bitmap* bitmap = roaring_bitmap_from(1);

      uint32_t* offsets = NULL;
      size_t n_offsets = 0;

      bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
      bool expected[] = {};

      // Should return valid pointer but with no meaningful data
      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      roaring_bitmap_free(bitmap);
    }

    IT("Should handle large offsets correctly")
    {
      // Create bitmap with large values
      Bitmap* bitmap = roaring_bitmap_from(UINT32_MAX - 1, UINT32_MAX);

      uint32_t offsets[] = { UINT32_MAX - 2, UINT32_MAX - 1, UINT32_MAX };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 0, 1, 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring_bitmap_free(bitmap);
    }

    IT("Should handle duplicate offsets in query")
    {
      // Create bitmap
      Bitmap* bitmap = roaring_bitmap_from(10);

      uint32_t offsets[] = { 10, 10, 20, 10 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 1, 1, 0, 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring_bitmap_free(bitmap);
    }

    IT("Should handle offsets in non-ascending order")
    {
      // Create bitmap
      Bitmap* bitmap = roaring_bitmap_from(1, 5, 10);

      uint32_t offsets[] = { 10, 1, 20, 5 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 1, 1, 0, 1 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring_bitmap_free(bitmap);
    }

    IT("Should work with dense bitmap")
    {
      // Create dense bitmap (range of consecutive values)
      Bitmap* bitmap = roaring_bitmap_create();
      for (uint32_t i = 100; i < 200; i++) {
        roaring_bitmap_add(bitmap, i);
      }

      uint32_t offsets[] = { 99, 100, 150, 199, 200 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap_getbits(bitmap, n_offsets, offsets);
      bool expected[] = { 0, 1, 1, 1, 0 };

      ASSERT_NOT_NULL(results);
      ASSERT_ARRAY_EQ(expected, results, ARRAY_LENGTH(expected), n_offsets);

      SAFE_FREE(results);
      roaring_bitmap_free(bitmap);
    }

    IT("Should handle NULL bitmap gracefully")
    {
      uint32_t offsets[] = { 1, 2, 3 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool* results = bitmap_getbits(NULL, n_offsets, offsets);
      ASSERT_NULL(results);
    }
  }
}
