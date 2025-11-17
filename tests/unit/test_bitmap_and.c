#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_and() {
  DESCRIBE("bitmap_and")
  {
    IT("Should perform an AND between three bitmaps")
    {
      Bitmap* twelve = bitmap_alloc();
      bitmap_setbit(twelve, 2, 1);
      bitmap_setbit(twelve, 3, 1);
      Bitmap* four = bitmap_alloc();
      bitmap_setbit(four, 2, 1);
      Bitmap* six = bitmap_alloc();
      bitmap_setbit(six, 1, 1);
      bitmap_setbit(six, 2, 1);

      Bitmap* bitmaps[] = { twelve, four, six };
      Bitmap* and = bitmap_alloc();
      bitmap_and(and, sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap**) bitmaps);
      roaring_uint32_iterator_t* iterator = roaring_iterator_create(and);
      int expected[] = { 2 };
      for (int i = 0; iterator->has_value; i++) {
        ASSERT(iterator->current_value == expected[i], "expect item[%zu] to be %llu. Received: %u", i, iterator->current_value, expected[i]);
        roaring_uint32_iterator_advance(iterator);
      }
      roaring_uint32_iterator_free(iterator);

      bitmap_free(and);

      bitmap_free(six);
      bitmap_free(four);
      bitmap_free(twelve);
    }

    IT("Should clear destination bitmap when passed zero bitmaps even if dest was not empty initially")
    {
      // Create a destination bitmap with some initial values
      uint32_t initial_values[] = { 10, 20, 30, 40, 50 };
      Bitmap* result = roaring_bitmap_of_ptr(ARRAY_LENGTH(initial_values), initial_values);

      // Call bitmap_one with zero bitmaps
      bitmap_and(result, 0, NULL);

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
      bitmap_setbit(bitmap2, 10, 1);
      bitmap_setbit(bitmap2, 100, 1);
      bitmap_setbit(bitmap2, 200, 1);

      // Use bitmap1 as both the result and one of the inputs
      const Bitmap* bitmaps[] = { bitmap1, bitmap2 };
      bitmap_and(bitmap1, 2, bitmaps);

      // Verify the result contains only common bits
      ASSERT(bitmap_getbit(bitmap1, 1) == 0, "bitmap1 should not contain bit 1");
      ASSERT(bitmap_getbit(bitmap1, 10) == 1, "bitmap1 should contain bit 10");
      ASSERT(bitmap_getbit(bitmap1, 100) == 1, "bitmap1 should contain bit 100");
      ASSERT(bitmap_getbit(bitmap1, 200) == 0, "bitmap1 should not contain bit 200");
      ASSERT_BITMAP_SIZE(2, bitmap1);

      bitmap_free(bitmap1);
      bitmap_free(bitmap2);
    }
  }
}
