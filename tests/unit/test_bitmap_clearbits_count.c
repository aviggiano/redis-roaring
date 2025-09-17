#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_clearbits_count() {
  DESCRIBE("bitmap_clearbits_count")
  {
    IT("Should return 0 when bitmap is NULL")
    {
      uint32_t offsets[] = { 1, 2, 3 };
      size_t result = bitmap_clearbits_count(NULL, 3, offsets);
      ASSERT_EQ(0, result);
    }

    IT("Should return 0 when offsets is NULL")
    {
      Bitmap* bitmap = roaring_bitmap_from(1, 2);
      size_t result = bitmap_clearbits_count(bitmap, 2, NULL);
      ASSERT_EQ(0, result);
      roaring_bitmap_free(bitmap);
    }

    IT("Should return 0 when n_offsets is 0")
    {
      Bitmap* bitmap = roaring_bitmap_from(1);
      uint32_t offsets[] = { 1 };

      size_t result = bitmap_clearbits_count(bitmap, 0, offsets);
      ASSERT_EQ(0, result);

      roaring_bitmap_free(bitmap);
    }

    IT("Should return 0 when bitmap is empty")
    {
      Bitmap* bitmap = roaring_bitmap_create();
      uint32_t offsets[] = { 1, 2, 3 };

      size_t result = bitmap_clearbits_count(bitmap, ARRAY_LENGTH(offsets), offsets);
      ASSERT_EQ(0, result);

      roaring_bitmap_free(bitmap);
    }

    IT("Should return correct count when all offsets exist in bitmap")
    {
      uint32_t offsets[] = { 10, 20, 30 };
      size_t n_offsets = ARRAY_LENGTH(offsets);
      Bitmap* bitmap = roaring_bitmap_of_ptr(n_offsets, offsets);
      size_t result = bitmap_clearbits_count(bitmap, n_offsets, offsets);

      ASSERT_EQ(n_offsets, result);
      ASSERT_BITMAP_SIZE(0, bitmap);

      roaring_bitmap_free(bitmap);
    }

    IT("Should return correct count when some offsets exist in bitmap")
    {
      Bitmap* bitmap = roaring_bitmap_from(10, 30);
      uint32_t offsets[] = { 10, 20, 30, 40 };
      size_t result = bitmap_clearbits_count(bitmap, ARRAY_LENGTH(offsets), offsets);

      ASSERT_EQ(2, result);
      ASSERT_BITMAP_SIZE(0, bitmap);

      roaring_bitmap_free(bitmap);
    }

    IT("Should return 0 when no offsets exist in bitmap")
    {
      Bitmap* bitmap = roaring_bitmap_from(100, 200);
      uint32_t offsets[] = { 10, 20, 30 };
      size_t n_offset = ARRAY_LENGTH(offsets);
      size_t result = bitmap_clearbits_count(bitmap, n_offset, offsets);

      ASSERT_EQ(0, result);
      ASSERT_TRUE(roaring_bitmap_contains(bitmap, 100));
      ASSERT_TRUE(roaring_bitmap_contains(bitmap, 200));

      roaring_bitmap_free(bitmap);
    }

    IT("Should handle duplicate offsets correctly")
    {
      Bitmap* bitmap = roaring_bitmap_from(10, 20);
      uint32_t offsets[] = { 10, 10, 20, 20 };
      size_t n_offset = ARRAY_LENGTH(offsets);
      size_t result = bitmap_clearbits_count(bitmap, n_offset, offsets);

      // Should only count each bit once, even if offset appears multiple times
      ASSERT_EQ(2, result);

      ASSERT_FALSE(roaring_bitmap_contains(bitmap, 10));
      ASSERT_FALSE(roaring_bitmap_contains(bitmap, 20));

      roaring_bitmap_free(bitmap);
    }

    IT("Should handle large offsets")
    {
      Bitmap* bitmap = roaring_bitmap_from(UINT32_MAX - 1, UINT32_MAX);
      uint32_t offsets[] = { UINT32_MAX - 1, UINT32_MAX };
      size_t n_offset = ARRAY_LENGTH(offsets);
      size_t result = bitmap_clearbits_count(bitmap, n_offset, offsets);

      ASSERT_EQ(2, result);
      ASSERT_FALSE(roaring_bitmap_contains(bitmap, UINT32_MAX - 1));
      ASSERT_FALSE(roaring_bitmap_contains(bitmap, UINT32_MAX));

      roaring_bitmap_free(bitmap);
    }
  }
}
