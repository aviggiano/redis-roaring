#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_from_bit_array() {
  DESCRIBE("bitmap_from_int_array")
  {
    IT("Should create a bitmap from a bit array and get the bit from the bitmap")
    {
      char array[] = "010101010010010010100110100111010010101010100101010101111101001001010100";
      size_t array_len = sizeof(array) / sizeof(*array) - 1;
      Bitmap* bitmap = bitmap_from_bit_array(array_len, array);

      size_t n;
      char* found = bitmap_get_bit_array(bitmap, &n);

      ASSERT_EQ(n, array_len - 2);

      ASSERT_ARRAY_EQ(array, found, array_len - 2, n);

      bitmap_free_bit_array(found);
      bitmap_free(bitmap);
    }
  }
}
