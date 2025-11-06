#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_andnot() {
  DESCRIBE("bitmap_andnot")
  {
    IT("Should clear bitmap when n is 0")
    {
      Bitmap* result = roaring_bitmap_from(1, 2, 3);

      bitmap_andnot(result, 0, NULL);

      ASSERT_EQ(0, roaring_bitmap_get_cardinality(result));
      ASSERT_TRUE(roaring_bitmap_is_empty(result));

      roaring_bitmap_free(result);
    }

    IT("Should copy bitmap when n is 1")
    {
      Bitmap* result = roaring_bitmap_create();
      Bitmap* input = roaring_bitmap_from(10, 20, 30);

      const Bitmap* bitmaps[] = { input };
      bitmap_andnot(result, 1, bitmaps);

      ASSERT_BITMAP_EQ(input, result);

      roaring_bitmap_free(result);
      roaring_bitmap_free(input);
    }

    IT("Should compute A AND NOT B when n is 2")
    {
      Bitmap* result = roaring_bitmap_create();
      Bitmap* a = roaring_bitmap_from(1, 2, 3, 4);
      Bitmap* b = roaring_bitmap_from(2, 4);

      const Bitmap* bitmaps[] = { a, b };
      bitmap_andnot(result, 2, bitmaps);

      uint32_t expected[] = { 1, 3 };

      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(result);
      roaring_bitmap_free(a);
      roaring_bitmap_free(b);
    }

    IT("Should compute A AND NOT (B OR C OR D) when n > 2")
    {
      Bitmap* result = roaring_bitmap_create();
      Bitmap* a = roaring_bitmap_from(1, 2, 3, 4, 5, 6, 7, 8);
      Bitmap* b = roaring_bitmap_from(2, 3);
      Bitmap* c = roaring_bitmap_from(4, 5);
      Bitmap* d = roaring_bitmap_from(6, 7);

      // Result should be A AND NOT (B OR C OR D) = {1, 8}
      const Bitmap* bitmaps[] = { a, b, c, d };
      bitmap_andnot(result, 4, bitmaps);

      uint32_t expected[] = { 1, 8 };

      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(result);
      roaring_bitmap_free(a);
      roaring_bitmap_free(b);
      roaring_bitmap_free(c);
      roaring_bitmap_free(d);
    }

    IT("Should handle empty bitmaps")
    {
      Bitmap* result = roaring_bitmap_create();
      Bitmap* a = roaring_bitmap_from(1, 2);
      Bitmap* b = roaring_bitmap_create();
      Bitmap* c = roaring_bitmap_create();

      const Bitmap* bitmaps[] = { a, b, c };
      bitmap_andnot(result, 3, bitmaps);

      // Result should be A AND NOT (empty OR empty) = A
      uint32_t expected[] = { 1, 2 };

      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(result);
      roaring_bitmap_free(a);
      roaring_bitmap_free(b);
      roaring_bitmap_free(c);
    }

    IT("Should handle result being empty")
    {
      Bitmap* result = roaring_bitmap_create();
      Bitmap* a = roaring_bitmap_from(1, 2);
      Bitmap* b = roaring_bitmap_from(1, 2, 3);

      const Bitmap* bitmaps[] = { a, b };
      bitmap_andnot(result, 2, bitmaps);

      ASSERT_EQ(0, roaring_bitmap_get_cardinality(result));
      ASSERT_TRUE(roaring_bitmap_is_empty(result));

      roaring_bitmap_free(result);
      roaring_bitmap_free(a);
      roaring_bitmap_free(b);
    }

    IT("Should handle result bitmap with existing data")
    {
      Bitmap* result = roaring_bitmap_from(100, 200);
      Bitmap* a = roaring_bitmap_from(1, 2);
      Bitmap* b = roaring_bitmap_from(1);

      const Bitmap* bitmaps[] = { a, b };
      bitmap_andnot(result, 2, bitmaps);

      uint32_t expected[] = { 2 };

      ASSERT_BITMAP_EQ_ARRAY(expected, ARRAY_LENGTH(expected), result);

      roaring_bitmap_free(result);
      roaring_bitmap_free(a);
      roaring_bitmap_free(b);
    }

    IT("Should handle large number of bitmaps")
    {
      Bitmap* result = roaring_bitmap_create();
      Bitmap* a = roaring_bitmap_create();

      const int num_bitmaps = 10;
      Bitmap* others[num_bitmaps];
      const Bitmap* bitmaps[num_bitmaps + 1];

      // A = {0, 1, 2, ..., 19}
      for (uint32_t i = 0; i < 20; i++) {
        roaring_bitmap_add(a, i);
      }

      bitmaps[0] = a;

      // Each other bitmap removes 2 values
      for (int i = 0; i < num_bitmaps; i++) {
        others[i] = roaring_bitmap_create();
        roaring_bitmap_add(others[i], i * 2);
        roaring_bitmap_add(others[i], i * 2 + 1);
        bitmaps[i + 1] = others[i];
      }

      bitmap_andnot(result, num_bitmaps + 1, bitmaps);

      ASSERT_EQ(0, roaring_bitmap_get_cardinality(result));

      roaring_bitmap_free(result);
      roaring_bitmap_free(a);
      for (int i = 0; i < num_bitmaps; i++) {
        roaring_bitmap_free(others[i]);
      }
    }
  }
}
