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
  }
}
