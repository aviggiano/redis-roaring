#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_andor() {
  DESCRIBE("bitmap_andor")
  {
    IT("Should handle empty array (n = 0)")
    {
      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, 0, NULL);

      // Result should remain unchanged (empty)
      ASSERT_BITMAP_SIZE(0, result);

      roaring_bitmap_free(result);
    }

    IT("Should clear destination bitmap when passed zero bitmaps even if dest was not empty initially")
    {
      // Create a destination bitmap with some initial values
      uint32_t initial_values[] = { 10, 20, 30, 40, 50 };
      Bitmap* result = roaring_bitmap_of_ptr(ARRAY_LENGTH(initial_values), initial_values);

      // Call bitmap_one with zero bitmaps
      bitmap_andor(result, 0, NULL);

      // Verify destination is now empty
      ASSERT_BITMAP_SIZE(0, result);

      roaring_bitmap_free(result);
    }

    IT("Should handle single bitmap (n = 1)")
    {
      uint32_t values1[] = { 1, 2, 3, 4, 5 };
      Bitmap* bitmap1 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);
      const Bitmap* bitmaps[] = { bitmap1 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Should call bitmap_and with single bitmap, expect same result as input
      ASSERT_BITMAP_EQ(result, bitmap1);

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(result);
    }

    IT("Should perform AND-OR operation with two bitmaps")
    {
      // bitmap1: {1, 2, 3, 4}
      uint32_t values1[] = { 1, 2, 3, 4 };
      Bitmap* bitmap1 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);

      // bitmap2: {3, 4, 5, 6}
      uint32_t values2[] = { 3, 4, 5, 6 };
      Bitmap* bitmap2 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: bitmap1 AND bitmap2 = {3, 4}
      uint32_t expected[] = { 3, 4 };

      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(result);
    }

    IT("Should perform AND-OR operation with three bitmaps")
    {
      // bitmap1: {1, 2, 3}
      uint32_t values1[] = { 1, 2, 3 };
      Bitmap* bitmap1 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);

      // bitmap2: {2, 3, 4}
      uint32_t values2[] = { 2, 3, 4 };
      Bitmap* bitmap2 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);

      // bitmap3: {3, 4, 5}
      uint32_t values3[] = { 3, 4, 5 };
      Bitmap* bitmap3 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: bitmap1 AND (bitmap2 OR bitmap3)
      // bitmap2 OR bitmap3 = {2, 3, 4, 5}
      // bitmap1 AND {2, 3, 4, 5} = {2, 3}
      uint32_t expected[] = { 2, 3 };

      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap3);
      roaring_bitmap_free(result);
    }

    IT("Should handle disjoint bitmaps")
    {
      // bitmap1: {1, 2}
      uint32_t values1[] = { 1, 2 };
      Bitmap* bitmap1 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);

      // bitmap2: {3, 4}
      uint32_t values2[] = { 3, 4 };
      Bitmap* bitmap2 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);

      // bitmap3: {5, 6}
      uint32_t values3[] = { 5, 6 };
      Bitmap* bitmap3 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values3), values3);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: bitmap1 AND (bitmap2 OR bitmap3)
      // bitmap2 OR bitmap3 = {3, 4, 5, 6}
      // bitmap1 AND {3, 4, 5, 6} = {} (empty)
      ASSERT_BITMAP_SIZE(0, result);

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap3);
      roaring_bitmap_free(result);
    }

    IT("Should handle empty bitmaps in array")
    {
      // bitmap1: {1, 2, 3}
      uint32_t values1[] = { 1, 2, 3 };
      Bitmap* bitmap1 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);

      // bitmap2: empty
      Bitmap* bitmap2 = roaring_bitmap_create();

      // bitmap3: {2, 3, 4}
      uint32_t values3[] = { 2, 3, 4 };
      Bitmap* bitmap3 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values3), values3);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: bitmap1 AND (bitmap2 OR bitmap3)
      // bitmap2 OR bitmap3 = {2, 3, 4} (empty OR bitmap3)
      // bitmap1 AND {2, 3, 4} = {2, 3}
      uint32_t expected[] = { 2, 3 };

      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap3);
      roaring_bitmap_free(result);
    }

    IT("Should handle when first bitmap is empty")
    {
      // bitmap1: empty
      Bitmap* bitmap1 = roaring_bitmap_create();

      // bitmap2: {1, 2, 3}
      uint32_t values2[] = { 1, 2, 3 };
      Bitmap* bitmap2 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);

      // bitmap3: {2, 3, 4}
      uint32_t values3[] = { 2, 3, 4 };
      Bitmap* bitmap3 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values3), values3);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: empty AND (bitmap2 OR bitmap3) = empty
      ASSERT_BITMAP_SIZE(0, result);

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap3);
      roaring_bitmap_free(result);
    }

    IT("Should handle large number of bitmaps")
    {
      const size_t num_bitmaps = 10;
      Bitmap* bitmaps_array[num_bitmaps];
      const Bitmap* bitmaps_ptrs[num_bitmaps];

      // Create first bitmap with values 1-5
      uint32_t first_values[] = { 1, 2, 3, 4, 5 };
      bitmaps_array[0] = roaring_bitmap_of_ptr(ARRAY_LENGTH(first_values), first_values);
      bitmaps_ptrs[0] = bitmaps_array[0];

      // Create remaining bitmaps, each containing some overlapping values
      for (size_t i = 1; i < num_bitmaps; i++) {
        bitmaps_array[i] = roaring_bitmap_create();
        roaring_bitmap_add(bitmaps_array[i], i + 1); // Each bitmap has unique value
        roaring_bitmap_add(bitmaps_array[i], 3);     // All have value 3
        bitmaps_ptrs[i] = bitmaps_array[i];
      }

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, num_bitmaps, bitmaps_ptrs);

      // Expected: first bitmap AND (OR of all others)
      // All others will contain at least {3} and various other values
      // So the AND should contain at least {3}
      ASSERT_TRUE(roaring_bitmap_contains(result, 3));

      // Clean up
      for (size_t i = 0; i < num_bitmaps; i++) {
        roaring_bitmap_free(bitmaps_array[i]);
      }
      roaring_bitmap_free(result);
    }

    IT("Should not modify input bitmaps")
    {
      uint32_t values1[] = { 1, 2, 3 };
      Bitmap* bitmap1 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values1), values1);
      Bitmap* bitmap1_copy = roaring_bitmap_copy(bitmap1);

      uint32_t values2[] = { 2, 3, 4 };
      Bitmap* bitmap2 = roaring_bitmap_of_ptr(ARRAY_LENGTH(values2), values2);
      Bitmap* bitmap2_copy = roaring_bitmap_copy(bitmap2);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Verify input bitmaps are unchanged
      ASSERT_BITMAP_EQ(bitmap1_copy, bitmap1);
      ASSERT_BITMAP_EQ(bitmap2_copy, bitmap2);

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap1_copy);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap2_copy);
      roaring_bitmap_free(result);
    }

    IT("Should handle fuzzer-discovered memcpy overlap case (regression test)")
    {
      // This test reproduces a bug found by fuzzing that caused
      // memcpy-param-overlap when using lazy OR operations.
      // The specific pattern of values triggers CRoaring's union_vector16
      // optimization which had overlapping memory regions.
      //
      // Original crash: bitmap_andor -> roaring_bitmap_lazy_or_inplace ->
      // array_array_container_lazy_inplace_union -> union_vector16 (memcpy overlap)

      // Create bitmaps with the pattern that triggered the bug
      // The exact values don't matter as much as the internal representation
      Bitmap* bitmap1 = roaring_bitmap_create();
      Bitmap* bitmap2 = roaring_bitmap_create();
      Bitmap* bitmap3 = roaring_bitmap_create();

      // Add values that create specific array container patterns
      for (uint32_t i = 0; i < 100; i++) {
        roaring_bitmap_add(bitmap1, i);
        roaring_bitmap_add(bitmap2, i + 50);
        roaring_bitmap_add(bitmap3, i + 25);
      }

      const Bitmap* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap* result = roaring_bitmap_create();
      // This should not crash with memcpy-param-overlap error
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Verify result is correct: bitmap1 AND (bitmap2 OR bitmap3)
      // bitmap2 OR bitmap3 covers range [25, 149]
      // bitmap1 covers [0, 99]
      // Intersection is [25, 99] = 75 values
      ASSERT_BITMAP_SIZE(75, result);

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap3);
      roaring_bitmap_free(result);
    }
  }
}
