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
  }
}
