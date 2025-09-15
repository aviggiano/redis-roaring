#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_intersect() {

  DESCRIBE("bitmap_intersect")
  {
    IT("Should return true when bitmaps intersect in NONE mode")
    {
      // Create two bitmaps with overlapping elements
      Bitmap* b1 = roaring_bitmap_from(1, 2, 3);
      Bitmap* b2 = roaring_bitmap_from(3, 4, 5);

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_NONE);
      ASSERT_TRUE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return false when bitmaps don't intersect in NONE mode")
    {
      // Create two bitmaps with no overlapping elements
      Bitmap* b1 = roaring_bitmap_from(1, 2);
      Bitmap* b2 = roaring_bitmap_from(3, 4);

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_NONE);
      ASSERT_FALSE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return false when one bitmap is empty in NONE mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2);
      Bitmap* b2 = roaring_bitmap_create();

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_NONE);
      ASSERT_FALSE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return true when b2 is subset of b1 in ALL mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2, 3, 4);
      Bitmap* b2 = roaring_bitmap_from(2, 3);

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_ALL);
      ASSERT_TRUE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return false when b2 is not subset of b1 in ALL mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2);
      Bitmap* b2 = roaring_bitmap_from(2, 3); // 3 is not in b1

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_ALL);
      ASSERT_FALSE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return true when b2 equals b1 in ALL mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2, 3);
      Bitmap* b2 = roaring_bitmap_from(1, 2, 3);

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_ALL);
      ASSERT_TRUE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return true when b2 is empty in ALL mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2);
      Bitmap* b2 = roaring_bitmap_create(); // b2 remains empty (empty set is subset of any set)

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_ALL);
      ASSERT_TRUE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return true when b2 is strict subset of b1 in ALL_STRICT mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2, 3, 4);
      Bitmap* b2 = roaring_bitmap_from(2, 3);

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_ALL_STRICT);
      ASSERT_TRUE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return false when b2 equals b1 in ALL_STRICT mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2, 3);
      Bitmap* b2 = roaring_bitmap_from(1, 2, 3);

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_ALL_STRICT);
      ASSERT_FALSE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return false when b2 is not subset of b1 in ALL_STRICT mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2);
      Bitmap* b2 = roaring_bitmap_from(2, 3); // 3 is not in b1

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_ALL_STRICT);
      ASSERT_FALSE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return true when b2 is empty in ALL_STRICT mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2);
      Bitmap* b2 = roaring_bitmap_create(); // b2 remains empty (empty set is strict subset if b1 is non-empty)

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_ALL_STRICT);
      ASSERT_TRUE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return false for invalid mode")
    {
      Bitmap* b1 = roaring_bitmap_from(1);
      Bitmap* b2 = roaring_bitmap_from(1);

      bool result = bitmap_intersect(b1, b2, 999); // Invalid mode
      ASSERT_FALSE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should handle NULL pointers gracefully")
    {
      Bitmap* b1 = roaring_bitmap_create();

      bool result1 = bitmap_intersect(NULL, b1, BITMAP_INTERSECT_MODE_NONE);
      ASSERT_FALSE(result1);

      bool result2 = bitmap_intersect(b1, NULL, BITMAP_INTERSECT_MODE_NONE);
      ASSERT_FALSE(result2);

      bool result3 = bitmap_intersect(NULL, NULL, BITMAP_INTERSECT_MODE_NONE);
      ASSERT_FALSE(result3);

      roaring_bitmap_free(b1);
    }

    IT("Should work with large bitmaps")
    {
      Bitmap* b1 = roaring_bitmap_create();
      Bitmap* b2 = roaring_bitmap_create();

      // Add many elements to test performance and correctness
      for (uint32_t i = 0; i < 10000; i += 2) {
        roaring_bitmap_add(b1, i);
      }

      for (uint32_t i = 1000; i < 5000; i += 2) {
        roaring_bitmap_add(b2, i);
      }

      bool result = bitmap_intersect(b1, b2, BITMAP_INTERSECT_MODE_NONE);
      ASSERT_TRUE(result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }
  }
}
