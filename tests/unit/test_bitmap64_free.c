#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_free() {
  DESCRIBE("bitmap64_free")
  {
    IT("Should alloc and free a Bitmap")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      bitmap64_free(bitmap);
      ASSERT(1, "OK");
    }
  }
}
