#include <stdio.h>
#include <assert.h>
#include "data-structure.h"

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
    roaring_uint32_iterator_t* iterator = roaring_iterator_create(or);
    int expected[] = {0, 2, 3, 4};
    for (int i = 0; iterator->has_value; i++) {
      assert(iterator->current_value == expected[i]);
      roaring_uint32_iterator_advance(iterator);
    }
    roaring_uint32_iterator_free(iterator);

    bitmap_free(or);

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
    roaring_uint32_iterator_t* iterator = roaring_iterator_create(and);
    int expected[] = {2};
    for (int i = 0; iterator->has_value; i++) {
      assert(iterator->current_value == expected[i]);
      roaring_uint32_iterator_advance(iterator);
    }
    roaring_uint32_iterator_free(iterator);

    bitmap_free(and);

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
    roaring_uint32_iterator_t* iterator = roaring_iterator_create(xor);
    int expected[] = {1, 2, 3};
    for (int i = 0; iterator->has_value; i++) {
      assert(iterator->current_value == expected[i]);
      roaring_uint32_iterator_advance(iterator);
    }
    roaring_uint32_iterator_free(iterator);

    bitmap_free(xor);

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
      roaring_uint32_iterator_t* iterator = roaring_iterator_create(not_array);
      for (int i = 0; iterator->has_value; i++) {
        assert(iterator->current_value == expected[i]);
        roaring_uint32_iterator_advance(iterator);
      }
      roaring_uint32_iterator_free(iterator);
    }
    {
      roaring_uint32_iterator_t* iterator = roaring_iterator_create(not);
      for (int i = 0; iterator->has_value; i++) {
        assert(iterator->current_value == expected[i]);
        roaring_uint32_iterator_advance(iterator);
      }
      roaring_uint32_iterator_free(iterator);
    }

    bitmap_free(not);
    bitmap_free(not_array);

    bitmap_free(twelve);
  }

  {
    printf("Should get n-th element of a bitmap for n=1..cardinality\n");
    Bitmap* bitmap = bitmap_alloc();
    uint32_t array[] = {0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233,
                        377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711,
                        28657, 46368, 75025, 121393, 196418, 317811};
    size_t array_len = sizeof(array) / sizeof(*array);
    for (size_t i = 0; i < array_len; i++) {
      bitmap_setbit(bitmap, array[i], 1);
    }


    {
      int64_t element = bitmap_get_nth_element_present(bitmap, 0);
      assert(element == -1);
    }
    {
      for (size_t i = 0; i < array_len; i++) {
        int64_t element = bitmap_get_nth_element_present(bitmap, i + 1);
        assert(element == array[i]);
      }
    }
    {
      int64_t element = bitmap_get_nth_element_present(bitmap, array_len + 1);
      assert(element == -1);
    }

    bitmap_free(bitmap);
  }

  {
    printf("Should get n-th non existent element of a bitmap for n=1..cardinality\n");
    Bitmap* bitmap = bitmap_alloc();
    uint32_t array[] = {0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233,
                        377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711,
                        28657, 46368, 75025, 121393, 196418, 317811};
    size_t array_len = sizeof(array) / sizeof(*array);
    for (size_t i = 0; i < array_len; i++) {
      bitmap_setbit(bitmap, array[i], 1);
    }

    {
      int64_t element = bitmap_get_nth_element_not_present(bitmap, 0);
      assert(element == -1);
    }
    {
      for (size_t i = 0; i < 1000; i++) {
        int64_t element = bitmap_get_nth_element_not_present(bitmap, i + 1);
        int64_t element_check = bitmap_get_nth_element_not_present_slow(bitmap, i + 1);
        assert(element == element_check);
      }
    }
    {
      int64_t element = bitmap_get_nth_element_not_present(bitmap, array[array_len - 1]);
      assert(element == -1);
    }

    bitmap_free(bitmap);
  }

  {
    printf("Should create a bitmap from an int array and get the array from the bitmap\n");
    uint32_t array[] = {
      0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233,
      377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711,
      28657, 46368, 75025, 121393, 196418, 317811
    };
    size_t array_len = sizeof(array) / sizeof(*array);
    Bitmap* bitmap = bitmap_from_int_array(array_len, array);

    size_t n;
    uint32_t* found = bitmap_get_int_array(bitmap, &n);
    for (size_t i = 0; i < array_len; i++) {
      assert(array[i] == found[i]);
    }

    bitmap_free_int_array(found);
    bitmap_free(bitmap);
  }

  {
    printf("Should create a bitmap from a bit array and get the bit from the bitmap\n");
    char array[] = "010101010010010010100110100111010010101010100101010101111101001001010100";
    size_t array_len = sizeof(array) / sizeof(*array) - 1;
    Bitmap* bitmap = bitmap_from_bit_array(array_len, array);

    size_t n;
    char* found = bitmap_get_bit_array(bitmap, &n);
    for (size_t i = 0; found[i]; i++) {
      assert(found[i] == array[i]);
    }
    assert(n == array_len - 2);

    bitmap_free_bit_array(found);
    bitmap_free(bitmap);
  }

  {
    printf("Should serialize\n");
    uint32_t array[] = {317811, 196418, 121393, 233, 144, 89, 55, 34, 21};
    size_t array_len = sizeof(array) / sizeof(*array);
    Bitmap* bitmap = bitmap_from_int_array(array_len, array);

    size_t serialized_max_size = roaring_bitmap_size_in_bytes(bitmap);
    char* serialized_bitmap = malloc(serialized_max_size);
    size_t serialized_size = roaring_bitmap_serialize(bitmap, serialized_bitmap);

    free(serialized_bitmap);
    assert(serialized_size <= serialized_max_size);

    bitmap_free(bitmap);
  }

  return 0;
}
