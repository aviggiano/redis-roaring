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

  {
    printf("Should perform an OR between two bitmaps\n");
    Bitmap* four = bitmap_alloc();
    bitmap_setbit(four, 2, 1);
    Bitmap* nine = bitmap_alloc();
    bitmap_setbit(nine, 0, 1);
    bitmap_setbit(nine, 3, 1);

    Bitmap* bitmaps[] = {four, nine};
    Bitmap* or = bitmap_or(2, (const Bitmap**) bitmaps);
    roaring_uint32_iterator_t* iterator = roaring_create_iterator(or);
    int expected[] = {0, 2, 3};
    for (int i = 0; iterator->has_value; i++) {
      assert(iterator->current_value == expected[i]);
      roaring_advance_uint32_iterator(iterator);
    }
    roaring_free_uint32_iterator(iterator);

    bitmap_free(nine);
    bitmap_free(four);
  }

  printf("All tests passed\n");
  return 0;
}