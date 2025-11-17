#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_or() {
  DESCRIBE("bitmap_or")
  {
    IT("Should perform an OR between three bitmaps")
    {
      Bitmap* sixteen = bitmap_alloc();
      bitmap_setbit(sixteen, 4, 1);
      Bitmap* four = bitmap_alloc();
      bitmap_setbit(four, 2, 1);
      Bitmap* nine = bitmap_alloc();
      bitmap_setbit(nine, 0, 1);
      bitmap_setbit(nine, 3, 1);

      Bitmap* bitmaps[] = { sixteen, four, nine };
      Bitmap* or = bitmap_alloc();
      bitmap_or(or , sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap**) bitmaps);
      roaring_uint32_iterator_t* iterator = roaring_iterator_create(or );
      int expected[] = { 0, 2, 3, 4 };
      for (int i = 0; iterator->has_value; i++) {
        ASSERT(iterator->current_value == expected[i], "expect item[%zu] to be %llu. Received: %u", i, iterator->current_value, expected[i]);
        roaring_uint32_iterator_advance(iterator);
      }
      roaring_uint32_iterator_free(iterator);

      bitmap_free(or );

      bitmap_free(nine);
      bitmap_free(four);
      bitmap_free(sixteen);
    }

    IT("Should clear destination bitmap when passed zero bitmaps even if dest was not empty initially")
    {
      // Create a destination bitmap with some initial values
      Bitmap* result = roaring_bitmap_create();
      roaring_bitmap_add(result, 1);

      // Call bitmap_one with zero bitmaps
      bitmap_or(result, 0, NULL);

      // Verify destination is now empty
      ASSERT_BITMAP_SIZE(0, result);

      roaring_bitmap_free(result);
    }

    IT("Should handle result bitmap appearing in input array (regression test for memcpy-param-overlap)")
    {
      // This is a regression test for the memcpy-param-overlap bug found by fuzzing
      // The bug occurred when the result bitmap pointer appeared in the input array,
      // causing inplace operations to trigger memcpy overlap with shared containers

      Bitmap* bitmap1 = bitmap_alloc();
      bitmap_setbit(bitmap1, 1, 1);
      bitmap_setbit(bitmap1, 10, 1);
      bitmap_setbit(bitmap1, 100, 1);

      Bitmap* bitmap2 = bitmap_alloc();
      bitmap_setbit(bitmap2, 2, 1);
      bitmap_setbit(bitmap2, 20, 1);
      bitmap_setbit(bitmap2, 200, 1);

      // Use bitmap1 as both the result and one of the inputs
      const Bitmap* bitmaps[] = { bitmap1, bitmap2 };
      bitmap_or(bitmap1, 2, bitmaps);

      // Verify the result contains all bits from both bitmaps
      ASSERT(bitmap_getbit(bitmap1, 1) == 1, "bitmap1 should contain bit 1");
      ASSERT(bitmap_getbit(bitmap1, 2) == 1, "bitmap1 should contain bit 2");
      ASSERT(bitmap_getbit(bitmap1, 10) == 1, "bitmap1 should contain bit 10");
      ASSERT(bitmap_getbit(bitmap1, 20) == 1, "bitmap1 should contain bit 20");
      ASSERT(bitmap_getbit(bitmap1, 100) == 1, "bitmap1 should contain bit 100");
      ASSERT(bitmap_getbit(bitmap1, 200) == 1, "bitmap1 should contain bit 200");
      ASSERT_BITMAP_SIZE(6, bitmap1);

      bitmap_free(bitmap1);
      bitmap_free(bitmap2);
    }
  }
}
