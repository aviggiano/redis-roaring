#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_ornot() {
  DESCRIBE("bitmap64_ornot")
  {
    IT("Should clear bitmap when n is 0")
    {
      Bitmap64* result = roaring64_bitmap_from(1, 2);

      bitmap64_ornot(result, 0, NULL);

      ASSERT_EQ(0, roaring64_bitmap_get_cardinality(result));
      roaring64_bitmap_free(result);
    }

    IT("Should clear bitmap when n is 1")
    {
      Bitmap64* result = roaring64_bitmap_from(5);
      Bitmap64* bitmap1 = roaring64_bitmap_from(10);

      const Bitmap64* bitmaps[] = { bitmap1 };
      bitmap64_ornot(result, 1, bitmaps);

      ASSERT_EQ(0, roaring64_bitmap_get_cardinality(result));
      roaring64_bitmap_free(result);
      roaring64_bitmap_free(bitmap1);
    }

    IT("Should compute (B AND NOT A) when n is 2")
    {
      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* bitmap_a = roaring64_bitmap_from(1, 2, 3);
      Bitmap64* bitmap_b = roaring64_bitmap_from(2, 3, 4, 5);

      const Bitmap64* bitmaps[] = { bitmap_a, bitmap_b };
      bitmap64_ornot(result, 2, bitmaps);

      uint64_t expected[] = { 4, 5 };
      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(bitmap_a);
      roaring64_bitmap_free(bitmap_b);
    }

    IT("Should compute ((B OR C OR D) AND NOT A) when n > 2")
    {
      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* bitmap_a = roaring64_bitmap_from(1, 2, 5);
      Bitmap64* bitmap_b = roaring64_bitmap_from(2, 3);
      Bitmap64* bitmap_c = roaring64_bitmap_from(4, 5);
      Bitmap64* bitmap_d = roaring64_bitmap_from(6, 7);

      const Bitmap64* bitmaps[] = { bitmap_a, bitmap_b, bitmap_c, bitmap_d };
      bitmap64_ornot(result, 4, bitmaps);

      uint64_t expected[] = { 3, 4, 6, 7 };
      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(bitmap_a);
      roaring64_bitmap_free(bitmap_b);
      roaring64_bitmap_free(bitmap_c);
      roaring64_bitmap_free(bitmap_d);
    }

    IT("Should handle empty bitmaps")
    {
      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* bitmap_a = roaring64_bitmap_create();
      Bitmap64* bitmap_b = roaring64_bitmap_create();

      const Bitmap64* bitmaps[] = { bitmap_a, bitmap_b };
      bitmap64_ornot(result, 2, bitmaps);

      ASSERT_EQ(0, roaring64_bitmap_get_cardinality(result));

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(bitmap_a);
      roaring64_bitmap_free(bitmap_b);
    }

    IT("Should handle large values")
    {
      uint64_t large_val1 = 0xFFFFFFFFFFFFULL;
      uint64_t large_val2 = 0xFFFFFFFFFFFEULL;

      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* bitmap_a = roaring64_bitmap_from(large_val1);
      Bitmap64* bitmap_b = roaring64_bitmap_from(large_val1, large_val2);

      const Bitmap64* bitmaps[] = { bitmap_a, bitmap_b };
      bitmap64_ornot(result, 2, bitmaps);

      uint64_t expected[] = { large_val2 };
      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(bitmap_a);
      roaring64_bitmap_free(bitmap_b);
    }

    IT("Should overwrite existing result bitmap content")
    {
      Bitmap64* result = roaring64_bitmap_from(100, 200);
      Bitmap64* bitmap_a = roaring64_bitmap_from(1);
      Bitmap64* bitmap_b = roaring64_bitmap_from(2, 3);

      const Bitmap64* bitmaps[] = { bitmap_a, bitmap_b };
      bitmap64_ornot(result, 2, bitmaps);

      // Result should only contain elements from the operation, not original values
      uint64_t expected[] = { 2, 3 };
      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(bitmap_a);
      roaring64_bitmap_free(bitmap_b);
    }
  }
}
