#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_one() {
  DESCRIBE("bitmap64_one")
  {
    IT("Should handle empty bitmap array")
    {
      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 0, NULL);
      ASSERT_BITMAP64_SIZE(0, result);
      roaring64_bitmap_free(result);
    }

    IT("Should clear destination bitmap when passed zero bitmaps even if dest was not empty initially")
    {
      // Create a destination bitmap with some initial values
      uint64_t initial_values[] = { 10, 20, 30, 40, 50 };
      Bitmap64* result = roaring64_bitmap_of_ptr(ARRAY_LENGTH(initial_values), initial_values);

      // Call bitmap_one with zero bitmaps
      bitmap64_one(result, 0, NULL);

      // Verify destination is now empty
      ASSERT_BITMAP64_SIZE(0, result);

      roaring64_bitmap_free(result);
    }

    IT("Should handle single bitmap")
    {
      uint64_t values[] = { 1, 3, 5, 7 };
      Bitmap64* bitmap1 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values), values);
      const Bitmap64* bitmaps[] = { bitmap1 };

      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 1, bitmaps);

      ASSERT_BITMAP64_EQ_ARRAY(values, ARRAY_LENGTH(values), result);

      roaring64_bitmap_free(bitmap1);
      roaring64_bitmap_free(result);
    }

    IT("Should handle two non-overlapping bitmaps")
    {
      uint64_t values1[] = { 1, 3, 5 };
      uint64_t values2[] = { 2, 4, 6 };
      uint64_t expected[] = { 1, 2, 3, 4, 5, 6 };

      Bitmap64* bitmap1 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);
      Bitmap64* bitmap2 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);
      const Bitmap64* bitmaps[] = { bitmap1, bitmap2 };

      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 2, bitmaps);

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(bitmap1);
      roaring64_bitmap_free(bitmap2);
      roaring64_bitmap_free(result);
    }

    IT("Should handle two overlapping bitmaps")
    {
      uint64_t values1[] = { 1, 2, 3, 4 };
      uint64_t values2[] = { 3, 4, 5, 6 };
      uint64_t expected[] = { 1, 2, 5, 6 }; // Only bits that appear exactly once

      Bitmap64* bitmap1 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);
      Bitmap64* bitmap2 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);
      const Bitmap64* bitmaps[] = { bitmap1, bitmap2 };

      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 2, bitmaps);

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(bitmap1);
      roaring64_bitmap_free(bitmap2);
      roaring64_bitmap_free(result);
    }

    IT("Should handle three bitmaps - Redis example")
    {
      // From Redis documentation example:
      // key1: bits 0, 4, 5, 6 (0001 0111 in reverse bit order)
      // key2: bits 1, 5, 6     (0010 0110 in reverse bit order)  
      // key3: bits 2, 3, 5, 6, 7 (0100 1101 in reverse bit order)
      // Expected: bits 3, 4, 7 (0111 1000 in reverse bit order)
      uint64_t values1[] = { 0, 4, 5, 6 };
      uint64_t values2[] = { 1, 5, 6 };
      uint64_t values3[] = { 2, 3, 5, 6, 7 };
      uint64_t expected[] = { 0, 1, 2, 3, 4, 7 }; // Bits appearing exactly once

      Bitmap64* bitmap1 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);
      Bitmap64* bitmap2 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);
      Bitmap64* bitmap3 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values3), values3);
      const Bitmap64* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 3, bitmaps);

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(bitmap1);
      roaring64_bitmap_free(bitmap2);
      roaring64_bitmap_free(bitmap3);
      roaring64_bitmap_free(result);
    }

    IT("Should handle all bitmaps have same bits")
    {
      uint64_t values[] = { 10, 20, 30 };

      Bitmap64* bitmap1 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values), values);
      Bitmap64* bitmap2 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values), values);
      Bitmap64* bitmap3 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values), values);
      const Bitmap64* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 3, bitmaps);

      // All bits appear in multiple bitmaps, so result should be empty
      ASSERT_BITMAP64_SIZE(0, result);

      roaring64_bitmap_free(bitmap1);
      roaring64_bitmap_free(bitmap2);
      roaring64_bitmap_free(bitmap3);
      roaring64_bitmap_free(result);
    }

    IT("Should handle one empty bitmap among others")
    {
      uint64_t values1[] = { 1, 2, 3 };
      uint64_t values3[] = { 3, 4, 5 };
      uint64_t expected[] = { 1, 2, 4, 5 }; // Bits from non-empty bitmaps that don't overlap

      Bitmap64* bitmap1 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);
      Bitmap64* bitmap2 = roaring64_bitmap_create(); // Empty bitmap
      Bitmap64* bitmap3 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values3), values3);
      const Bitmap64* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 3, bitmaps);

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(bitmap1);
      roaring64_bitmap_free(bitmap2);
      roaring64_bitmap_free(bitmap3);
      roaring64_bitmap_free(result);
    }

    IT("Should handle large bit values")
    {
      uint64_t values1[] = { UINT64_MAX - 2, UINT64_MAX - 1 };
      uint64_t values2[] = { UINT64_MAX - 1, UINT64_MAX };
      uint64_t expected[] = { UINT64_MAX - 2, UINT64_MAX }; // Non-overlapping bits

      Bitmap64* bitmap1 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);
      Bitmap64* bitmap2 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);
      const Bitmap64* bitmaps[] = { bitmap1, bitmap2 };

      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 2, bitmaps);

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(bitmap1);
      roaring64_bitmap_free(bitmap2);
      roaring64_bitmap_free(result);
    }

    IT("Should handle many bitmaps with complex overlaps")
    {
      uint64_t values1[] = { 1, 2, 3, 4, 5 };
      uint64_t values2[] = { 2, 3, 4, 6, 7 };
      uint64_t values3[] = { 3, 4, 5, 7, 8 };
      uint64_t values4[] = { 4, 5, 6, 8, 9 };
      uint64_t expected[] = { 1, 9 }; // Only bits that appear exactly once

      Bitmap64* bitmap1 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);
      Bitmap64* bitmap2 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);
      Bitmap64* bitmap3 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values3), values3);
      Bitmap64* bitmap4 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values4), values4);
      const Bitmap64* bitmaps[] = { bitmap1, bitmap2, bitmap3, bitmap4 };

      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 4, bitmaps);

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(bitmap1);
      roaring64_bitmap_free(bitmap2);
      roaring64_bitmap_free(bitmap3);
      roaring64_bitmap_free(bitmap4);
      roaring64_bitmap_free(result);
    }

    IT("Should not modify input bitmaps")
    {
      uint64_t values1[] = { 1, 2, 3 };
      uint64_t values2[] = { 2, 3, 4 };

      Bitmap64* bitmap1 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);
      Bitmap64* bitmap2 = roaring64_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);
      const Bitmap64* bitmaps[] = { bitmap1, bitmap2 };

      Bitmap64* result = roaring64_bitmap_create();
      bitmap64_one(result, 2, bitmaps);

      // Check input bitmaps are unchanged
      ASSERT_BITMAP64_EQ_ARRAY(values1, ARRAY_LENGTH(values1), bitmap1);
      ASSERT_BITMAP64_EQ_ARRAY(values2, ARRAY_LENGTH(values2), bitmap2);

      roaring64_bitmap_free(bitmap1);
      roaring64_bitmap_free(bitmap2);
      roaring64_bitmap_free(result);
    }
  }
}
