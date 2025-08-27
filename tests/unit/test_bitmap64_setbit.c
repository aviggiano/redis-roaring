#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_setbit() {
  DESCRIBE("bitmap64_setbit")
  {
    IT("Should set bit to true when value is true")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t offset = 42;

      bitmap64_setbit(bitmap, offset, true);

      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, offset));
      ASSERT_EQ(1, bitmap64_get_cardinality(bitmap));

      bitmap64_free(bitmap);
    }

    IT("Should unset bit when value is false")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t offset = 42;

      // First set the bit
      bitmap64_setbit(bitmap, offset, true);
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, offset));

      // Then unset it
      bitmap64_setbit(bitmap, offset, false);
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, offset));
      ASSERT_EQ(0, bitmap64_get_cardinality(bitmap));

      bitmap64_free(bitmap);
    }

    IT("Should handle setting already set bit to true")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t offset = 100;

      // Set bit twice
      bitmap64_setbit(bitmap, offset, true);
      bitmap64_setbit(bitmap, offset, true);

      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, offset));
      ASSERT_EQ(1, bitmap64_get_cardinality(bitmap));

      bitmap64_free(bitmap);
    }

    IT("Should handle unsetting already unset bit")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t offset = 200;

      // Try to unset a bit that was never set
      bitmap64_setbit(bitmap, offset, false);

      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, offset));
      ASSERT_EQ(0, bitmap64_get_cardinality(bitmap));

      bitmap64_free(bitmap);
    }

    IT("Should handle offset zero")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t offset = 0;

      bitmap64_setbit(bitmap, offset, true);
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, offset));

      bitmap64_setbit(bitmap, offset, false);
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, offset));

      bitmap64_free(bitmap);
    }

    IT("Should handle large offset values")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t offset = UINT64_MAX - 1;

      bitmap64_setbit(bitmap, offset, true);
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, offset));

      bitmap64_setbit(bitmap, offset, false);
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, offset));

      bitmap64_free(bitmap);
    }

    IT("Should handle maximum uint64_t offset")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t offset = UINT64_MAX;

      bitmap64_setbit(bitmap, offset, true);
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, offset));

      bitmap64_setbit(bitmap, offset, false);
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, offset));

      bitmap64_free(bitmap);
    }

    IT("Should handle multiple bits operations")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t offsets[] = { 1, 100, 1000, 10000, 100000 };
      int num_offsets = sizeof(offsets) / sizeof(offsets[0]);

      // Set all bits
      for (int i = 0; i < num_offsets; i++) {
        bitmap64_setbit(bitmap, offsets[i], true);
      }

      // Verify all bits are set
      for (int i = 0; i < num_offsets; i++) {
        ASSERT_TRUE(roaring64_bitmap_contains(bitmap, offsets[i]));
      }
      ASSERT_EQ(num_offsets, bitmap64_get_cardinality(bitmap));

      // Unset every other bit
      for (int i = 0; i < num_offsets; i += 2) {
        bitmap64_setbit(bitmap, offsets[i], false);
      }

      // Verify the pattern
      for (int i = 0; i < num_offsets; i++) {
        if (i % 2 == 0) {
          ASSERT_FALSE(roaring64_bitmap_contains(bitmap, offsets[i]));
        } else {
          ASSERT_TRUE(roaring64_bitmap_contains(bitmap, offsets[i]));
        }
      }

      bitmap64_free(bitmap);
    }

    IT("Should maintain bitmap integrity across operations")
    {
      Bitmap64* bitmap = bitmap64_alloc();

      // Perform a series of mixed operations
      bitmap64_setbit(bitmap, 1, true);
      bitmap64_setbit(bitmap, 2, true);
      bitmap64_setbit(bitmap, 3, true);
      ASSERT_EQ(3, bitmap64_get_cardinality(bitmap));

      bitmap64_setbit(bitmap, 2, false);
      ASSERT_EQ(2, bitmap64_get_cardinality(bitmap));
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 1));
      ASSERT_FALSE(roaring64_bitmap_contains(bitmap, 2));
      ASSERT_TRUE(roaring64_bitmap_contains(bitmap, 3));

      bitmap64_setbit(bitmap, 2, true);
      bitmap64_setbit(bitmap, 4, true);
      ASSERT_EQ(4, bitmap64_get_cardinality(bitmap));

      bitmap64_free(bitmap);
    }
  }
}
