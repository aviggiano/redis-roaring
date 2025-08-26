#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_not() {
  DESCRIBE("bitmap_not")
  {
    IT("Should perform a NOT using two different methods")
    {
      Bitmap* twelve = bitmap_alloc();
      bitmap_setbit(twelve, 2, 1);
      bitmap_setbit(twelve, 3, 1);

      Bitmap* bitmaps[] = { twelve };
      Bitmap* not_array = bitmap_not_array(sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap**) bitmaps);
      Bitmap * not = bitmap_not(bitmaps[0]);
      int expected[] = { 0, 1 };

      {
        roaring_uint32_iterator_t* iterator = roaring_iterator_create(not_array);
        for (int i = 0; iterator->has_value; i++) {
          ASSERT(iterator->current_value == expected[i], "expect item[%zu] to be %llu. Received: %u", i, iterator->current_value, expected[i]);
          roaring_uint32_iterator_advance(iterator);
        }
        roaring_uint32_iterator_free(iterator);
      }
      {
        roaring_uint32_iterator_t* iterator = roaring_iterator_create(not);
        for (int i = 0; iterator->has_value; i++) {
          ASSERT(iterator->current_value == expected[i], "expect item[%zu] to be %llu. Received: %u", i, iterator->current_value, expected[i]);
          roaring_uint32_iterator_advance(iterator);
        }
        roaring_uint32_iterator_free(iterator);
      }

      bitmap_free(not);
      bitmap_free(not_array);

      bitmap_free(twelve);
    }
  }
}
