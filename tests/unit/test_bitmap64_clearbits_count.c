#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_clearbits_count() {
  DESCRIBE("bitmap64_clearbits_count")
  {
    IT("Should return 0 when bitmap is NULL")
    {
      uint64_t offsets[] = { 1, 2, 3 };
      size_t result = bitmap64_clearbits_count(NULL, 3, offsets);
      ASSERT_EQ(0, result);
    }

    IT("Should return 0 when offsets is NULL")
    {
      Bitmap64* bitmap = roaring64_bitmap_from(1, 2);
      size_t result = bitmap64_clearbits_count(bitmap, 2, NULL);
      ASSERT_EQ(0, result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should return 0 when n_offsets is 0")
    {
      Bitmap64* bitmap = roaring64_bitmap_from(1);
      uint64_t offsets[] = { 1 };

      size_t result = bitmap64_clearbits_count(bitmap, 0, offsets);
      ASSERT_EQ(0, result);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should return 0 when bitmap is empty")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t offsets[] = { 1, 2, 3 };

      size_t result = bitmap64_clearbits_count(bitmap, ARRAY_LENGTH(offsets), offsets);
      ASSERT_EQ(0, result);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should return correct count when all offsets exist in bitmap")
    {
      uint64_t offsets[] = { 10, 20, 30 };
      size_t n_offsets = ARRAY_LENGTH(offsets);
      Bitmap64* bitmap = roaring64_bitmap_of_ptr(n_offsets, offsets);
      size_t result = bitmap64_clearbits_count(bitmap, n_offsets, offsets);

      ASSERT_EQ(n_offsets, result);
      ASSERT_BITMAP64_SIZE(0, bitmap);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should return correct count when some offsets exist in bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_from(10, 30);
      uint64_t offsets[] = { 10, 20, 30, 40 };
      size_t result = bitmap64_clearbits_count(bitmap, ARRAY_LENGTH(offsets), offsets);

      ASSERT_EQ(2, result);
      ASSERT_BITMAP64_SIZE(0, bitmap);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should return 0 when no offsets exist in bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_from(100, 200);
      uint64_t offsets[] = { 10, 20, 30 };
      size_t n_offset = ARRAY_LENGTH(offsets);
      size_t result = bitmap64_clearbits_count(bitmap, n_offset, offsets);

      ASSERT_EQ(0, result);
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 100));
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 200));

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle duplicate offsets correctly")
    {
      Bitmap64* bitmap = roaring64_bitmap_from(10, 20);
      uint64_t offsets[] = { 10, 10, 20, 20 };
      size_t n_offset = ARRAY_LENGTH(offsets);
      size_t result = bitmap64_clearbits_count(bitmap, n_offset, offsets);

      // Should only count each bit once, even if offset appears multiple times
      ASSERT_EQ(2, result);

      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 10));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 20));

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle large offsets")
    {
      Bitmap64* bitmap = roaring64_bitmap_from(UINT64_MAX - 1, UINT64_MAX);
      uint64_t offsets[] = { UINT64_MAX - 1, UINT64_MAX };
      size_t n_offset = ARRAY_LENGTH(offsets);
      size_t result = bitmap64_clearbits_count(bitmap, n_offset, offsets);

      ASSERT_EQ(2, result);
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, UINT64_MAX - 1));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, UINT64_MAX));

      roaring64_bitmap_free(bitmap);
    }
  }
}
