#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_from_bit_array() {
  DESCRIBE("bitmap64_from_bit_array")
  {
    IT("Should create empty bitmap from empty array")
    {
      const char* empty_array = "";
      Bitmap64* bitmap = bitmap64_from_bit_array(0, empty_array);

      ASSERT_NOT_NULL(bitmap);
      ASSERT_EQ(0, roaring64_bitmap_get_cardinality(bitmap));

      roaring64_bitmap_free(bitmap);
    }

    IT("Should create bitmap with single bit set")
    {
      const char* array = "1";
      Bitmap64* bitmap = bitmap64_from_bit_array(1, array);

      ASSERT_NOT_NULL(bitmap);
      ASSERT_EQ(1, roaring64_bitmap_get_cardinality(bitmap));
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 0));

      roaring64_bitmap_free(bitmap);
    }

    IT("Should create bitmap with single bit unset")
    {
      const char* array = "0";
      Bitmap64* bitmap = bitmap64_from_bit_array(1, array);

      ASSERT_NOT_NULL(bitmap);
      ASSERT_EQ(0, roaring64_bitmap_get_cardinality(bitmap));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 0));

      roaring64_bitmap_free(bitmap);
    }

    IT("Should correctly set multiple bits")
    {
      const char* array = "101010";
      Bitmap64* bitmap = bitmap64_from_bit_array(6, array);

      ASSERT_NOT_NULL(bitmap);
      ASSERT_EQ(3, roaring64_bitmap_get_cardinality(bitmap));

      // Check which bits are set (indices 0, 2, 4)
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 0));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 1));
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 2));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 3));
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 4));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 5));

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle all bits set")
    {
      const char* array = "1111111";
      Bitmap64* bitmap = bitmap64_from_bit_array(7, array);

      ASSERT_NOT_NULL(bitmap);
      ASSERT_EQ(7, roaring64_bitmap_get_cardinality(bitmap));

      // Check all bits are set
      for (uint64_t i = 0; i < 7; i++) {
        ASSERT_TRUE(roaring64_bitmap_contains(bitmap, i));
      }

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle all bits unset")
    {
      const char* array = "0000000";
      Bitmap64* bitmap = bitmap64_from_bit_array(7, array);

      ASSERT_NOT_NULL(bitmap);
      ASSERT_EQ(0, roaring64_bitmap_get_cardinality(bitmap));

      // Check all bits are unset
      for (uint64_t i = 0; i < 7; i++) {
        ASSERT_FALSE(roaring64_bitmap_contains(bitmap, i));
      }

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle large arrays")
    {
      size_t size = 1000;
      char* large_array = malloc(size + 1);

      // Create pattern: every 3rd bit is set
      for (size_t i = 0; i < size; i++) {
        large_array[i] = (i % 3 == 0) ? '1' : '0';
      }
      large_array[size] = '\0';

      Bitmap64* bitmap = bitmap64_from_bit_array(size, large_array);

      ASSERT_NOT_NULL(bitmap);

      // Expected cardinality: bits at indices 0, 3, 6, 9, ... (334 total)
      size_t expected_cardinality = (size + 2) / 3;  // ceiling division
      ASSERT_EQ(expected_cardinality, roaring64_bitmap_get_cardinality(bitmap));

      // Verify pattern
      for (size_t i = 0; i < size; i++) {
        if (i % 3 == 0) {
          ASSERT_TRUE(roaring64_bitmap_contains(bitmap, i));
        } else {
          ASSERT_FALSE(roaring64_bitmap_contains(bitmap, i));
        }
      }

      SAFE_FREE(large_array);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should ignore non-'1' characters")
    {
      const char* array = "1x0y1z0";
      Bitmap64* bitmap = bitmap64_from_bit_array(7, array);

      ASSERT_NOT_NULL(bitmap);
      ASSERT_EQ(2, roaring64_bitmap_get_cardinality(bitmap));

      // Only indices 0 and 4 should be set (where '1' appears)
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 0));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 1));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 2));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 3));
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 4));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 5));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 6));

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle size parameter correctly when larger than array")
    {
      const char* array = "101";
      // Size is larger than actual array length
      Bitmap64* bitmap = bitmap64_from_bit_array(5, array);

      ASSERT_NOT_NULL(bitmap);

      // Should process first 3 characters normally, then undefined behavior
      // for indices 3 and 4 (depends on memory content after the string)
      // At minimum, we know indices 0 and 2 should be set
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 0));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 1));
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 2));

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle very large indices")
    {
      size_t size = 100000;
      char* large_array = calloc(size + 1, sizeof(char));

      // Set only the last bit
      for (size_t i = 0; i < size - 1; i++) {
        large_array[i] = '0';
      }
      large_array[size - 1] = '1';
      large_array[size] = '\0';

      Bitmap64* bitmap = bitmap64_from_bit_array(size, large_array);

      ASSERT_NOT_NULL(bitmap);
      ASSERT_EQ(1, roaring64_bitmap_get_cardinality(bitmap));
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, size - 1));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 0));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, size / 2));

      SAFE_FREE(large_array);
      roaring64_bitmap_free(bitmap);
    }
  }
}
