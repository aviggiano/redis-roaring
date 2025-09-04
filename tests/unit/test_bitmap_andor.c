#include "data-structure.h"
#include "../test-utils.h"

// Helper function to create a bitmap with specific values
static Bitmap* create_test_bitmap(uint32_t* values, size_t count) {
  Bitmap* bitmap = roaring_bitmap_create();
  for (size_t i = 0; i < count; i++) {
    roaring_bitmap_add(bitmap, values[i]);
  }
  return bitmap;
}

// Helper function to check if two bitmaps are equal
static bool bitmaps_equal(const Bitmap* a, const Bitmap* b) {
  return roaring_bitmap_equals(a, b);
}

void test_bitmap_andor() {

  DESCRIBE("bitmap_andor")
  {
    IT("Should handle empty array (n = 0)")
    {
      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, 0, NULL);

      // Result should remain unchanged (empty)
      ASSERT_EQ(0, roaring_bitmap_get_cardinality(result));

      roaring_bitmap_free(result);
    }

    IT("Should handle single bitmap (n = 1)")
    {
      uint32_t values1[] = { 1, 2, 3, 4, 5 };
      Bitmap* bitmap1 = create_test_bitmap(values1, 5);
      const Bitmap* bitmaps[] = { bitmap1 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Should call bitmap_and with single bitmap, expect same result as input
      ASSERT_TRUE(bitmaps_equal(result, bitmap1));

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(result);
    }

    IT("Should perform AND-OR operation with two bitmaps")
    {
      // bitmap1: {1, 2, 3, 4}
      uint32_t values1[] = { 1, 2, 3, 4 };
      Bitmap* bitmap1 = create_test_bitmap(values1, 4);

      // bitmap2: {3, 4, 5, 6}
      uint32_t values2[] = { 3, 4, 5, 6 };
      Bitmap* bitmap2 = create_test_bitmap(values2, 4);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: bitmap1 AND bitmap2 = {3, 4}
      Bitmap* expected = roaring_bitmap_create();
      roaring_bitmap_add(expected, 3);
      roaring_bitmap_add(expected, 4);

      ASSERT_TRUE(bitmaps_equal(result, expected));

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(result);
      roaring_bitmap_free(expected);
    }

    IT("Should perform AND-OR operation with three bitmaps")
    {
      // bitmap1: {1, 2, 3}
      uint32_t values1[] = { 1, 2, 3 };
      Bitmap* bitmap1 = create_test_bitmap(values1, 3);

      // bitmap2: {2, 3, 4}
      uint32_t values2[] = { 2, 3, 4 };
      Bitmap* bitmap2 = create_test_bitmap(values2, 3);

      // bitmap3: {3, 4, 5}
      uint32_t values3[] = { 3, 4, 5 };
      Bitmap* bitmap3 = create_test_bitmap(values3, 3);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: bitmap1 AND (bitmap2 OR bitmap3)
      // bitmap2 OR bitmap3 = {2, 3, 4, 5}
      // bitmap1 AND {2, 3, 4, 5} = {2, 3}
      Bitmap* expected = roaring_bitmap_create();
      roaring_bitmap_add(expected, 2);
      roaring_bitmap_add(expected, 3);

      ASSERT_TRUE(bitmaps_equal(result, expected));

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap3);
      roaring_bitmap_free(result);
      roaring_bitmap_free(expected);
    }

    IT("Should handle disjoint bitmaps")
    {
      // bitmap1: {1, 2}
      uint32_t values1[] = { 1, 2 };
      Bitmap* bitmap1 = create_test_bitmap(values1, 2);

      // bitmap2: {3, 4}
      uint32_t values2[] = { 3, 4 };
      Bitmap* bitmap2 = create_test_bitmap(values2, 2);

      // bitmap3: {5, 6}
      uint32_t values3[] = { 5, 6 };
      Bitmap* bitmap3 = create_test_bitmap(values3, 2);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: bitmap1 AND (bitmap2 OR bitmap3)
      // bitmap2 OR bitmap3 = {3, 4, 5, 6}
      // bitmap1 AND {3, 4, 5, 6} = {} (empty)
      ASSERT_EQ(0, roaring_bitmap_get_cardinality(result));

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap3);
      roaring_bitmap_free(result);
    }

    IT("Should handle empty bitmaps in array")
    {
      // bitmap1: {1, 2, 3}
      uint32_t values1[] = { 1, 2, 3 };
      Bitmap* bitmap1 = create_test_bitmap(values1, 3);

      // bitmap2: empty
      Bitmap* bitmap2 = roaring_bitmap_create();

      // bitmap3: {2, 3, 4}
      uint32_t values3[] = { 2, 3, 4 };
      Bitmap* bitmap3 = create_test_bitmap(values3, 3);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: bitmap1 AND (bitmap2 OR bitmap3)
      // bitmap2 OR bitmap3 = {2, 3, 4} (empty OR bitmap3)
      // bitmap1 AND {2, 3, 4} = {2, 3}
      Bitmap* expected = roaring_bitmap_create();
      roaring_bitmap_add(expected, 2);
      roaring_bitmap_add(expected, 3);

      ASSERT_TRUE(bitmaps_equal(result, expected));

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap3);
      roaring_bitmap_free(result);
      roaring_bitmap_free(expected);
    }

    IT("Should handle when first bitmap is empty")
    {
      // bitmap1: empty
      Bitmap* bitmap1 = roaring_bitmap_create();

      // bitmap2: {1, 2, 3}
      uint32_t values2[] = { 1, 2, 3 };
      Bitmap* bitmap2 = create_test_bitmap(values2, 3);

      // bitmap3: {2, 3, 4}
      uint32_t values3[] = { 2, 3, 4 };
      Bitmap* bitmap3 = create_test_bitmap(values3, 3);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2, bitmap3 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Expected: empty AND (bitmap2 OR bitmap3) = empty
      ASSERT_EQ(0, roaring_bitmap_get_cardinality(result));

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
      bitmaps_array[0] = create_test_bitmap(first_values, 5);
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
      Bitmap* bitmap1 = create_test_bitmap(values1, 3);
      Bitmap* bitmap1_copy = roaring_bitmap_copy(bitmap1);

      uint32_t values2[] = { 2, 3, 4 };
      Bitmap* bitmap2 = create_test_bitmap(values2, 3);
      Bitmap* bitmap2_copy = roaring_bitmap_copy(bitmap2);

      const Bitmap* bitmaps[] = { bitmap1, bitmap2 };

      Bitmap* result = roaring_bitmap_create();
      bitmap_andor(result, ARRAY_LENGTH(bitmaps), bitmaps);

      // Verify input bitmaps are unchanged
      ASSERT_TRUE(bitmaps_equal(bitmap1, bitmap1_copy));
      ASSERT_TRUE(bitmaps_equal(bitmap2, bitmap2_copy));

      roaring_bitmap_free(bitmap1);
      roaring_bitmap_free(bitmap1_copy);
      roaring_bitmap_free(bitmap2);
      roaring_bitmap_free(bitmap2_copy);
      roaring_bitmap_free(result);
    }
  }
}
