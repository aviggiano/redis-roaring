#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_or() {
  DESCRIBE("bitmap64_or")
  {
    IT("Should perform an OR between three bitmaps")
    {
      Bitmap64* sixteen = bitmap64_alloc();
      bitmap64_setbit(sixteen, 4, 1);
      Bitmap64* four = bitmap64_alloc();
      bitmap64_setbit(four, 2, 1);
      Bitmap64* nine = bitmap64_alloc();
      bitmap64_setbit(nine, 0, 1);
      bitmap64_setbit(nine, 3, 1);

      Bitmap64* bitmaps[] = { sixteen, four, nine };
      Bitmap64* or = bitmap64_alloc();
      bitmap64_or(or , sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap64**) bitmaps);
      roaring64_iterator_t* iterator = roaring64_iterator_create(or );
      uint64_t expected[] = { 0, 2, 3, 4 };
      for (int i = 0; roaring64_iterator_has_value(iterator); i++) {
        ASSERT(roaring64_iterator_value(iterator) == expected[i], "expect item[%zu] to be %llu. Received: %u", i, roaring64_iterator_value(iterator), expected[i]);
        roaring64_iterator_advance(iterator);
      }
      roaring64_iterator_free(iterator);

      bitmap64_free(or );
      bitmap64_free(nine);
      bitmap64_free(four);
      bitmap64_free(sixteen);
    }

    IT("Should clear destination bitmap when passed zero bitmaps even if dest was not empty initially")
    {
      // Create a destination bitmap with some initial values
      Bitmap64* result = roaring64_bitmap_create();
      roaring64_bitmap_add(result, 1);

      // Call bitmap_one with zero bitmaps
      bitmap64_or(result, 0, NULL);

      // Verify destination is now empty
      ASSERT_BITMAP64_SIZE(0, result);

      roaring64_bitmap_free(result);
    }
  }
}
