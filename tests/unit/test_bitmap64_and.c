#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_and() {
  DESCRIBE("bitmap64_and")
  {
    IT("Should perform an AND between three bitmaps")
    {
      Bitmap64* twelve = bitmap64_alloc();
      bitmap64_setbit(twelve, 2, 1);
      bitmap64_setbit(twelve, 3, 1);
      Bitmap64* four = bitmap64_alloc();
      bitmap64_setbit(four, 2, 1);
      Bitmap64* six = bitmap64_alloc();
      bitmap64_setbit(six, 1, 1);
      bitmap64_setbit(six, 2, 1);

      Bitmap64* bitmaps[] = { twelve, four, six };
      Bitmap64* and = bitmap64_alloc();
      bitmap64_and(and, sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap64**) bitmaps);
      roaring64_iterator_t* iterator = roaring64_iterator_create(and);
      uint64_t expected[] = { 2 };
      for (uint64_t i = 0; roaring64_iterator_has_value(iterator); i++) {
        ASSERT(roaring64_iterator_value(iterator) == expected[i], "expect item[%zu] to be %llu. Received: %u", i, roaring64_iterator_value(iterator), expected[i]);
        roaring64_iterator_advance(iterator);
      }
      roaring64_iterator_free(iterator);

      bitmap64_free(and);
      bitmap64_free(six);
      bitmap64_free(four);
      bitmap64_free(twelve);
    }

    IT("Should clear destination bitmap when passed zero bitmaps even if dest was not empty initially")
    {
      // Create a destination bitmap with some initial values
      Bitmap64* result = roaring64_bitmap_create();
      roaring64_bitmap_add(result, 1);

      // Call bitmap_one with zero bitmaps
      bitmap64_and(result, 0, NULL);

      // Verify destination is now empty
      ASSERT_BITMAP64_SIZE(0, result);

      roaring64_bitmap_free(result);
    }
  }
}
