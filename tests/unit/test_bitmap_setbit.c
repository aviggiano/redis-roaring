#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_setbit() {
  DESCRIBE("bitmap_setbit")
  {
    IT("Should set bit with value 1/0 on multiple offsets and get bit equals to 1/0")
    {
      for (char bit = 0; bit <= 1; bit++) {
        for (uint32_t offset = 0; offset < 100; offset++) {
          Bitmap* bitmap = bitmap_alloc();
          bitmap_setbit(bitmap, offset, bit);
          char value = bitmap_getbit(bitmap, offset);
          bitmap_free(bitmap);
          ASSERT(value == bit, "Expect offset = %d where bit = %d", offset, bit);
        }
      }
    }
  }
}
