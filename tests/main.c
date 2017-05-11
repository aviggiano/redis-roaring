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
    printf("Should perform an OR between three bitmaps\n");
    Bitmap* sixteen = bitmap_alloc();
    bitmap_setbit(sixteen, 4, 1);
    Bitmap* four = bitmap_alloc();
    bitmap_setbit(four, 2, 1);
    Bitmap* nine = bitmap_alloc();
    bitmap_setbit(nine, 0, 1);
    bitmap_setbit(nine, 3, 1);

    Bitmap* bitmaps[] = {sixteen, four, nine};
    Bitmap* or = bitmap_or(sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap**) bitmaps);
    roaring_uint32_iterator_t* iterator = roaring_create_iterator(or);
    int expected[] = {0, 2, 3, 4};
    for (int i = 0; iterator->has_value; i++) {
      assert(iterator->current_value == expected[i]);
      roaring_advance_uint32_iterator(iterator);
    }
    roaring_free_uint32_iterator(iterator);

    bitmap_free(nine);
    bitmap_free(four);
    bitmap_free(sixteen);
  }


  {
    printf("Should perform an AND between three bitmaps\n");
    Bitmap* twelve = bitmap_alloc();
    bitmap_setbit(twelve, 2, 1);
    bitmap_setbit(twelve, 3, 1);
    Bitmap* four = bitmap_alloc();
    bitmap_setbit(four, 2, 1);
    Bitmap* six = bitmap_alloc();
    bitmap_setbit(six, 1, 1);
    bitmap_setbit(six, 2, 1);

    Bitmap* bitmaps[] = {twelve, four, six};
    Bitmap* and = bitmap_and(sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap**) bitmaps);
    roaring_uint32_iterator_t* iterator = roaring_create_iterator(and);
    int expected[] = {2};
    for (int i = 0; iterator->has_value; i++) {
      assert(iterator->current_value == expected[i]);
      roaring_advance_uint32_iterator(iterator);
    }
    roaring_free_uint32_iterator(iterator);

    bitmap_free(six);
    bitmap_free(four);
    bitmap_free(twelve);
  }


  {
    printf("Should perform a XOR between three bitmaps\n");
    Bitmap* twelve = bitmap_alloc();
    bitmap_setbit(twelve, 2, 1);
    bitmap_setbit(twelve, 3, 1);
    Bitmap* four = bitmap_alloc();
    bitmap_setbit(four, 2, 1);
    Bitmap* six = bitmap_alloc();
    bitmap_setbit(six, 1, 1);
    bitmap_setbit(six, 2, 1);

    Bitmap* bitmaps[] = {twelve, four, six};
    Bitmap* xor = bitmap_xor(sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap**) bitmaps);
    roaring_uint32_iterator_t* iterator = roaring_create_iterator(xor);
    int expected[] = {1, 2, 3};
    for (int i = 0; iterator->has_value; i++) {
      assert(iterator->current_value == expected[i]);
      roaring_advance_uint32_iterator(iterator);
    }
    roaring_free_uint32_iterator(iterator);

    bitmap_free(six);
    bitmap_free(four);
    bitmap_free(twelve);
  }

  {
    printf("Should perform a NOT using two different methods\n");
    Bitmap* twelve = bitmap_alloc();
    bitmap_setbit(twelve, 2, 1);
    bitmap_setbit(twelve, 3, 1);

    Bitmap* bitmaps[] = {twelve};
    Bitmap* not_array = bitmap_not_array(sizeof(bitmaps) / sizeof(*bitmaps), (const Bitmap**) bitmaps);
    Bitmap* not = bitmap_not(bitmaps[0]);
    int expected[] = {0, 1};

    {
      roaring_uint32_iterator_t* iterator = roaring_create_iterator(not_array);
      for (int i = 0; iterator->has_value; i++) {
        assert(iterator->current_value == expected[i]);
        roaring_advance_uint32_iterator(iterator);
      }
      roaring_free_uint32_iterator(iterator);
    }
    {
      roaring_uint32_iterator_t* iterator = roaring_create_iterator(not);
      for (int i = 0; iterator->has_value; i++) {
        assert(iterator->current_value == expected[i]);
        roaring_advance_uint32_iterator(iterator);
      }
      roaring_free_uint32_iterator(iterator);
    }

    bitmap_free(not);
    bitmap_free(not_array);

    bitmap_free(twelve);
  }

  printf("All tests passed\n");
  return 0;
}