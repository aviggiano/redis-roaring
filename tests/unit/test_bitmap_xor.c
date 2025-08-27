#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_xor() {
  DESCRIBE("bitmap_xor")
  {
    IT("Should perform a XOR between three bitmaps")
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
      Bitmap* xor = bitmap_xor(sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap**) bitmaps);
      roaring_uint32_iterator_t* iterator = roaring_iterator_create(xor);
      int expected[] = { 1, 2, 3 };
      for (int i = 0; iterator->has_value; i++) {
        ASSERT(iterator->current_value == expected[i], "expect item[%zu] to be %llu. Received: %u", i, iterator->current_value, expected[i]);
        roaring_uint32_iterator_advance(iterator);
      }
      roaring_uint32_iterator_free(iterator);

      bitmap_free(xor);

      bitmap_free(six);
      bitmap_free(four);
      bitmap_free(twelve);
    }
  }
}
