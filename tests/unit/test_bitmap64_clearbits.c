#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_clearbits() {
  DESCRIBE("bitmap64_clearbits")
  {
    IT("Should return false when bitmap is NULL")
    {
      uint64_t offsets[] = { 1, 2, 3 };
      size_t n_offsets = ARRAY_LENGTH(offsets);

      bool result = bitmap64_clearbits(NULL, n_offsets, offsets);

      ASSERT_FALSE(result);
    }

    IT("Should return true when bitmap is valid and offsets is NULL")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();

      bool result = bitmap64_clearbits(bitmap, 0, NULL);

      ASSERT_TRUE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should return true when n_offsets is 0")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t offsets[] = { 1, 2, 3 };

      bool result = bitmap64_clearbits(bitmap, 0, offsets);

      ASSERT_TRUE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should clear single bit from bitmap")
    {
      uint64_t offsets[] = { 42 };
      size_t n_offsets = ARRAY_LENGTH(offsets);
      Bitmap64* bitmap = roaring64_bitmap_from(42);

      bool result = bitmap64_clearbits(bitmap, n_offsets, offsets);

      ASSERT_TRUE(result);
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, offsets[0]));
      roaring64_bitmap_free(bitmap);
    }

    IT("Should clear multiple bits from bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_from(10, 20, 30, 40, 50);
      uint64_t bits_to_clear[] = { 20, 40 };
      uint64_t expected[] = { 10, 30, 50 };
      size_t n_clear = ARRAY_LENGTH(bits_to_clear);

      // Clear some bits
      bool result = bitmap64_clearbits(bitmap, n_clear, bits_to_clear);

      ASSERT_TRUE(result);
      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), bitmap);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle large offset values")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t large_offset = UINT64_MAX - 100;

      // Set a large offset bit
      roaring64_bitmap_add(bitmap, large_offset);
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, large_offset));

      // Clear the large offset bit
      bool result = bitmap64_clearbits(bitmap, 1, &large_offset);

      ASSERT_TRUE(result);
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, large_offset));

      roaring64_bitmap_free(bitmap);
    }
  }
}
