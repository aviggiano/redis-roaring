#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_clearbits() {
  DESCRIBE("bitmap_clearbits")
  {
    IT("Should return false when bitmap is NULL")
    {
      uint32_t offsets[] = { 1, 2, 3 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool result = bitmap_clearbits(NULL, n_offsets, offsets);

      ASSERT_FALSE(result);
    }

    IT("Should return true when bitmap is valid and offsets is NULL")
    {
      Bitmap* bitmap = roaring_bitmap_create();

      bool result = bitmap_clearbits(bitmap, 0, NULL);

      ASSERT_TRUE(result);
      roaring_bitmap_free(bitmap);
    }

    IT("Should return true when n_offsets is 0")
    {
      Bitmap* bitmap = roaring_bitmap_create();
      uint32_t offsets[] = { 1, 2, 3 };

      bool result = bitmap_clearbits(bitmap, 0, offsets);

      ASSERT_TRUE(result);
      roaring_bitmap_free(bitmap);
    }

    IT("Should clear single bit from bitmap")
    {
      uint32_t offsets[] = { 42 };
      size_t n_offsets = ARRAY_LENGTH(offsets);
      Bitmap* bitmap = roaring_bitmap_from(42);

      bool result = bitmap_clearbits(bitmap, n_offsets, offsets);

      ASSERT_TRUE(result);
      ASSERT_FALSE(roaring_bitmap_contains(bitmap, offsets[0]));
      roaring_bitmap_free(bitmap);
    }

    IT("Should clear multiple bits from bitmap")
    {
      Bitmap* bitmap = roaring_bitmap_from(10, 20, 30, 40, 50);
      uint32_t bits_to_clear[] = { 20, 40 };
      uint32_t expected[] = { 10, 30, 50 };
      size_t n_clear = ARRAY_LENGTH(bits_to_clear);

      // Clear some bits
      bool result = bitmap_clearbits(bitmap, n_clear, bits_to_clear);

      ASSERT_TRUE(result);
      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), bitmap);

      roaring_bitmap_free(bitmap);
    }

    IT("Should handle large offset values")
    {
      Bitmap* bitmap = roaring_bitmap_create();
      uint32_t large_offset = UINT32_MAX - 100;

      // Set a large offset bit
      roaring_bitmap_add(bitmap, large_offset);
      ASSERT_TRUE(roaring_bitmap_contains(bitmap, large_offset));

      // Clear the large offset bit
      bool result = bitmap_clearbits(bitmap, 1, &large_offset);

      ASSERT_TRUE(result);
      ASSERT_FALSE(roaring_bitmap_contains(bitmap, large_offset));

      roaring_bitmap_free(bitmap);
    }
  }
}
