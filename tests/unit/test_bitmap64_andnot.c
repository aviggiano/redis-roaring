#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_andnot() {
  DESCRIBE("bitmap64_andnot")
  {
    IT("Should clear bitmap when n is 0")
    {
      Bitmap64* result = roaring64_bitmap_from(1, 2, 3);

      bitmap64_andnot(result, 0, NULL);

      ASSERT_EQ(0, roaring64_bitmap_get_cardinality(result));
      ASSERT_TRUE(roaring64_bitmap_is_empty(result));

      roaring64_bitmap_free(result);
    }

    IT("Should copy bitmap when n is 1")
    {
      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* input = roaring64_bitmap_from(10, 20, 30);

      const Bitmap64* bitmaps[] = { input };
      bitmap64_andnot(result, 1, bitmaps);

      ASSERT_BITMAP64_EQ(input, result);

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(input);
    }

    IT("Should compute A AND NOT B when n is 2")
    {
      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* a = roaring64_bitmap_from(1, 2, 3, 4);
      Bitmap64* b = roaring64_bitmap_from(2, 4);

      const Bitmap64* bitmaps[] = { a, b };
      bitmap64_andnot(result, 2, bitmaps);

      uint64_t expected[] = { 1, 3 };

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(a);
      roaring64_bitmap_free(b);
    }

    IT("Should compute A AND NOT (B OR C OR D) when n > 2")
    {
      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* a = roaring64_bitmap_from(1, 2, 3, 4, 5, 6, 7, 8);
      Bitmap64* b = roaring64_bitmap_from(2, 3);
      Bitmap64* c = roaring64_bitmap_from(4, 5);
      Bitmap64* d = roaring64_bitmap_from(6, 7);

      // Result should be A AND NOT (B OR C OR D) = {1, 8}
      const Bitmap64* bitmaps[] = { a, b, c, d };
      bitmap64_andnot(result, 4, bitmaps);

      uint64_t expected[] = { 1, 8 };

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(a);
      roaring64_bitmap_free(b);
      roaring64_bitmap_free(c);
      roaring64_bitmap_free(d);
    }

    IT("Should handle empty bitmaps")
    {
      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* a = roaring64_bitmap_from(1, 2);
      Bitmap64* b = roaring64_bitmap_create();
      Bitmap64* c = roaring64_bitmap_create();

      const Bitmap64* bitmaps[] = { a, b, c };
      bitmap64_andnot(result, 3, bitmaps);

      // Result should be A AND NOT (empty OR empty) = A
      uint64_t expected[] = { 1, 2 };

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(a);
      roaring64_bitmap_free(b);
      roaring64_bitmap_free(c);
    }

    IT("Should handle result being empty")
    {
      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* a = roaring64_bitmap_from(1, 2);
      Bitmap64* b = roaring64_bitmap_from(1, 2, 3);

      const Bitmap64* bitmaps[] = { a, b };
      bitmap64_andnot(result, 2, bitmaps);

      ASSERT_EQ(0, roaring64_bitmap_get_cardinality(result));
      ASSERT_TRUE(roaring64_bitmap_is_empty(result));

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(a);
      roaring64_bitmap_free(b);
    }

    IT("Should handle result bitmap with existing data")
    {
      Bitmap64* result = roaring64_bitmap_from(100, 200);
      Bitmap64* a = roaring64_bitmap_from(1, 2);
      Bitmap64* b = roaring64_bitmap_from(1);

      const Bitmap64* bitmaps[] = { a, b };
      bitmap64_andnot(result, 2, bitmaps);

      uint64_t expected[] = { 2 };

      ASSERT_BITMAP64_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(a);
      roaring64_bitmap_free(b);
    }

    IT("Should handle large number of bitmaps")
    {
      Bitmap64* result = roaring64_bitmap_create();
      Bitmap64* a = roaring64_bitmap_create();

      const int num_bitmaps = 10;
      Bitmap64* others[num_bitmaps];
      const Bitmap64* bitmaps[num_bitmaps + 1];

      // A = {0, 1, 2, ..., 19}
      for (uint64_t i = 0; i < 20; i++) {
        roaring64_bitmap_add(a, i);
      }

      bitmaps[0] = a;

      // Each other bitmap removes 2 values
      for (int i = 0; i < num_bitmaps; i++) {
        others[i] = roaring64_bitmap_create();
        roaring64_bitmap_add(others[i], i * 2);
        roaring64_bitmap_add(others[i], i * 2 + 1);
        bitmaps[i + 1] = others[i];
      }

      bitmap64_andnot(result, num_bitmaps + 1, bitmaps);

      ASSERT_EQ(0, roaring64_bitmap_get_cardinality(result));

      roaring64_bitmap_free(result);
      roaring64_bitmap_free(a);
      for (int i = 0; i < num_bitmaps; i++) {
        roaring64_bitmap_free(others[i]);
      }
    }
  }
}
