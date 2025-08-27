#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_not() {
  DESCRIBE("bitmap64_not")
  {
    IT("Should perform a NOT using two different methods")
    {
      Bitmap64* twelve = bitmap64_alloc();
      bitmap64_setbit(twelve, 2, 1);
      bitmap64_setbit(twelve, 3, 1);

      Bitmap64* bitmaps[] = { twelve };
      Bitmap64* not_array = bitmap64_not_array(sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap64**) bitmaps);
      Bitmap64 * not = bitmap64_not(bitmaps[0]);
      int expected[] = { 0, 1 };

      {
        roaring64_iterator_t* iterator = roaring64_iterator_create(not_array);
        for (uint64_t i = 0; roaring64_iterator_has_value(iterator); i++) {
          ASSERT(roaring64_iterator_value(iterator) == expected[i], "expect item[%zu] to be %llu. Received: %u", i, roaring64_iterator_value(iterator), expected[i]);
          roaring64_iterator_advance(iterator);
        }
        roaring64_iterator_free(iterator);
      }
      {
        roaring64_iterator_t* iterator = roaring64_iterator_create(not);
        for (int i = 0; roaring64_iterator_has_value(iterator); i++) {
          ASSERT(roaring64_iterator_value(iterator) == expected[i], "expect item[%zu] to be %llu. Received: %u", i, roaring64_iterator_value(iterator), expected[i]);
          roaring64_iterator_advance(iterator);
        }
        roaring64_iterator_free(iterator);
      }

      bitmap64_free(not);
      bitmap64_free(not_array);
      bitmap64_free(twelve);
    }
  }
}
