#include <math.h>
#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_jaccard() {
  DESCRIBE("bitmap_jaccard")
  {
    IT("Should return 0 when first bitmap is NULL")
    {
      Bitmap* b2 = roaring_bitmap_from(1);
      double result = bitmap_jaccard(NULL, b2);
      ASSERT_EQ(0.0, result);
      roaring_bitmap_free(b2);
    }

    IT("Should return 0 when second bitmap is NULL")
    {
      Bitmap* b1 = roaring_bitmap_from(1);
      double result = bitmap_jaccard(b1, NULL);
      ASSERT_EQ(0.0, result);
      roaring_bitmap_free(b1);
    }

    IT("Should return 0 when both bitmaps are NULL")
    {
      double result = bitmap_jaccard(NULL, NULL);
      ASSERT_EQ(0.0, result);
    }

    IT("Should return 1 when comparing same bitmap instance")
    {
      Bitmap* b1 = roaring_bitmap_from(1);

      double result = bitmap_jaccard(b1, b1);
      ASSERT_EQ(1.0, result);

      roaring_bitmap_free(b1);
    }

    IT("Should return -1 when both bitmaps are empty")
    {
      Bitmap* b1 = roaring_bitmap_create();
      Bitmap* b2 = roaring_bitmap_create();

      double result = bitmap_jaccard(b1, b2);
      ASSERT_EQ(-1.0, result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return 0 when bitmaps have no intersection")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2, 3, 4);
      Bitmap* b2 = roaring_bitmap_create();

      double result = bitmap_jaccard(b1, b2);
      ASSERT_EQ(0.0, result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should return 1 when bitmaps are identical but different instances")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2, 3);
      Bitmap* b2 = roaring_bitmap_from(1, 2, 3);

      double result = bitmap_jaccard(b1, b2);
      ASSERT_EQ(1.0, result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should calculate correct Jaccard index for partial overlap")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2, 3);
      Bitmap* b2 = roaring_bitmap_from(2, 3, 4);

      // Intersection = {2, 3} (size = 2)
      // Union = {1, 2, 3, 4} (size = 4)
      // Jaccard = 2/4 = 0.5
      double result = bitmap_jaccard(b1, b2);
      ASSERT_EQ(0.5, result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should handle one empty bitmap")
    {
      Bitmap* b1 = roaring_bitmap_from(1, 2);
      Bitmap* b2 = roaring_bitmap_create(); // b2 remains empty

      // Intersection = {} (size = 0)
      // Union = {1, 2} (size = 2)
      // Jaccard = 0/2 = 0.0
      double result = bitmap_jaccard(b1, b2);
      ASSERT_EQ(0.0, result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }

    IT("Should handle large bitmaps with complex overlap")
    {
      Bitmap* b1 = roaring_bitmap_create();
      Bitmap* b2 = roaring_bitmap_create();

      // Add many elements to b1
      for (uint32_t i = 0; i < 1000; i++) {
        roaring_bitmap_add(b1, i);
      }

      // Add overlapping elements to b2
      for (uint32_t i = 500; i < 1500; i++) {
        roaring_bitmap_add(b2, i);
      }

      // Intersection = {500, 501, ..., 999} (size = 500)
      // Union = {0, 1, ..., 1499} (size = 1500)
      // Jaccard = 500/1500 = 1/3 ≈ 0.3333...
      double result = bitmap_jaccard(b1, b2);
      double expected = 500.0 / 1500.0;

      ASSERT(fabs(result - expected) < 0.0001,
        "Expected %f, got %f", expected, result);

      roaring_bitmap_free(b1);
      roaring_bitmap_free(b2);
    }
  }
}
