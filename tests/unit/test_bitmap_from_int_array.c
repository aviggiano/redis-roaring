#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_from_int_array() {
  DESCRIBE("bitmap_from_int_array")
  {
    IT("Should create a bitmap from an int array and get the array from the bitmap")
    {
      uint32_t array[] = {
        0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233,
        377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711,
        28657, 46368, 75025, 121393, 196418, 317811
      };
      size_t array_len = sizeof(array) / sizeof(*array);
      Bitmap* bitmap = bitmap_from_int_array(array_len, array);

      size_t n;
      uint32_t* found = bitmap_get_int_array(bitmap, &n);

      ASSERT_ARRAY_EQ(array, found, array_len, n);

      bitmap_free_int_array(found);
      bitmap_free(bitmap);
    }

    IT("Should serialize")
    {
      uint32_t array[] = { 317811, 196418, 121393, 233, 144, 89, 55, 34, 21 };
      size_t array_len = sizeof(array) / sizeof(*array);
      Bitmap* bitmap = bitmap_from_int_array(array_len, array);

      size_t serialized_max_size = roaring_bitmap_size_in_bytes(bitmap);
      char* serialized_bitmap = malloc(serialized_max_size);
      size_t serialized_size = roaring_bitmap_serialize(bitmap, serialized_bitmap);

      free(serialized_bitmap);
      ASSERT_TRUE(serialized_size <= serialized_max_size);

      bitmap_free(bitmap);
    }
  }
}
