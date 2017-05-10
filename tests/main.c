#include <stdio.h>
#include <assert.h>
#include "data_structure.h"

int main(int argc, char* argv[]) {
  {
    printf("Should alloc and free a Bitmap\n");
    Bitmap* bitmap = bitmap_alloc();
    bitmap_free(bitmap);
    assert(1);
  }

  {
    printf("Should set bit with value 1/0 on multiple offsets and get bit equals to 1/0\n");
    for (char bit = 0; bit <= 1; bit++) {
      for (uint32_t offset = 0; offset < 100; offset++) {
        Bitmap* bitmap = bitmap_alloc();
        bitmap_setbit(bitmap, offset, bit);
        char value = bitmap_getbit(bitmap, offset);
        bitmap_free(bitmap);
        assert(value == bit);
      }
    }
  }

  printf("All tests passed\n");
  return 0;
}