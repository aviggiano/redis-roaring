#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_getbit() {
  DESCRIBE("bitmap64_getbit")
  {
    IT("Should return true for bit that exists in bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t test_offset = 42;
      roaring64_bitmap_add(bitmap, test_offset);

      bool result = bitmap64_getbit(bitmap, test_offset);
      ASSERT_TRUE(result);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should return false for bit that does not exist in bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t test_offset = 123;

      bool result = bitmap64_getbit(bitmap, test_offset);
      ASSERT_FALSE(result);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should return false for empty bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t test_offset = 0;

      bool result = bitmap64_getbit(bitmap, test_offset);
      ASSERT_FALSE(result);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle offset 0 correctly when bit is set")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t test_offset = 0;
      roaring64_bitmap_add(bitmap, test_offset);

      bool result = bitmap64_getbit(bitmap, test_offset);
      ASSERT_TRUE(result);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle maximum uint64_t offset correctly")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t test_offset = UINT64_MAX;
      roaring64_bitmap_add(bitmap, test_offset);

      bool result = bitmap64_getbit(bitmap, test_offset);
      ASSERT_TRUE(result);

      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle large offset values correctly")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t test_offset = 0xFFFFFFFFULL;
      roaring64_bitmap_add(bitmap, test_offset);

      bool result = bitmap64_getbit(bitmap, test_offset);
      ASSERT_TRUE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should work with multiple bits set in bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t offsets[] = { 1, 100, 1000, 10000, 100000 };

      // Add multiple offsets
      for (int i = 0; i < ARRAY_LENGTH(offsets); i++) {
        roaring64_bitmap_add(bitmap, offsets[i]);
      }

      // Test each offset
      for (int i = 0; i < ARRAY_LENGTH(offsets); i++) {
        bool result = bitmap64_getbit(bitmap, offsets[i]);
        ASSERT_TRUE(result);
      }

      roaring64_bitmap_free(bitmap);
    }

    IT("Should return false after bit is removed from bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t test_offset = 555;
      roaring64_bitmap_add(bitmap, test_offset);

      // Verify bit is initially set
      bool initial_result = bitmap64_getbit(bitmap, test_offset);
      ASSERT_TRUE(initial_result);

      // Remove the bit
      roaring64_bitmap_remove(bitmap, test_offset);

      bool result = bitmap64_getbit(bitmap, test_offset);
      ASSERT_FALSE(result);

      roaring64_bitmap_free(bitmap);
    }
  }
}
