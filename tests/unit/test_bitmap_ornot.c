#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_ornot() {
  DESCRIBE("bitmap_ornot")
  {
    IT("Should clear bitmap when n is 0")
    {
      Bitmap* result = roaring_bitmap_from(1, 2);

      bitmap_ornot(result, 0, NULL);

      ASSERT_EQ(0, roaring_bitmap_get_cardinality(result));
      roaring_bitmap_free(result);
    }

    IT("Should clear bitmap when n is 1")
    {
      Bitmap* result = roaring_bitmap_from(5);
      Bitmap* bitmap1 = roaring_bitmap_from(10);

      const Bitmap* bitmaps[] = { bitmap1 };
      bitmap_ornot(result, 1, bitmaps);

      ASSERT_EQ(0, roaring_bitmap_get_cardinality(result));
      roaring_bitmap_free(result);
      roaring_bitmap_free(bitmap1);
    }

    IT("Should compute (B AND NOT A) when n is 2")
    {
      Bitmap* result = roaring_bitmap_create();
      Bitmap* bitmap_a = roaring_bitmap_from(1, 2, 3);
      Bitmap* bitmap_b = roaring_bitmap_from(2, 3, 4, 5);

      const Bitmap* bitmaps[] = { bitmap_a, bitmap_b };
      bitmap_ornot(result, 2, bitmaps);

      uint32_t expected[] = { 4, 5 };
      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(result);
      roaring_bitmap_free(bitmap_a);
      roaring_bitmap_free(bitmap_b);
    }

    IT("Should compute ((B OR C OR D) AND NOT A) when n > 2")
    {
      Bitmap* result = roaring_bitmap_create();
      Bitmap* bitmap_a = roaring_bitmap_from(1, 2, 5);
      Bitmap* bitmap_b = roaring_bitmap_from(2, 3);
      Bitmap* bitmap_c = roaring_bitmap_from(4, 5);
      Bitmap* bitmap_d = roaring_bitmap_from(6, 7);

      const Bitmap* bitmaps[] = { bitmap_a, bitmap_b, bitmap_c, bitmap_d };
      bitmap_ornot(result, 4, bitmaps);

      uint32_t expected[] = { 3, 4, 6, 7 };
      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(result);
      roaring_bitmap_free(bitmap_a);
      roaring_bitmap_free(bitmap_b);
      roaring_bitmap_free(bitmap_c);
      roaring_bitmap_free(bitmap_d);
    }

    IT("Should handle empty bitmaps")
    {
      Bitmap* result = roaring_bitmap_create();
      Bitmap* bitmap_a = roaring_bitmap_create();
      Bitmap* bitmap_b = roaring_bitmap_create();

      const Bitmap* bitmaps[] = { bitmap_a, bitmap_b };
      bitmap_ornot(result, 2, bitmaps);

      ASSERT_EQ(0, roaring_bitmap_get_cardinality(result));

      roaring_bitmap_free(result);
      roaring_bitmap_free(bitmap_a);
      roaring_bitmap_free(bitmap_b);
    }

    IT("Should handle large values")
    {
      uint32_t large_val1 = 0xFFFFFFFFU;
      uint32_t large_val2 = 0xFFFFFFFEU;

      Bitmap* result = roaring_bitmap_create();
      Bitmap* bitmap_a = roaring_bitmap_from(large_val1);
      Bitmap* bitmap_b = roaring_bitmap_from(large_val1, large_val2);

      const Bitmap* bitmaps[] = { bitmap_a, bitmap_b };
      bitmap_ornot(result, 2, bitmaps);

      uint32_t expected[] = { large_val2 };
      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(result);
      roaring_bitmap_free(bitmap_a);
      roaring_bitmap_free(bitmap_b);
    }

    IT("Should overwrite existing result bitmap content")
    {
      Bitmap* result = roaring_bitmap_from(100, 200);
      Bitmap* bitmap_a = roaring_bitmap_from(1);
      Bitmap* bitmap_b = roaring_bitmap_from(2, 3);

      const Bitmap* bitmaps[] = { bitmap_a, bitmap_b };
      bitmap_ornot(result, 2, bitmaps);

      // Result should only contain elements from the operation, not original values
      uint32_t expected[] = { 2, 3 };
      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(result);
      roaring_bitmap_free(bitmap_a);
      roaring_bitmap_free(bitmap_b);
    }
  }
}
