#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_free() {
  DESCRIBE("bitmap_free")
  {
    IT("Should alloc and free a Bitmap")
    {
      Bitmap* bitmap = bitmap_alloc();
      bitmap_free(bitmap);
      ASSERT(1, "OK");
    }
  }
}
