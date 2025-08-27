#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap_get_nth_element() {
  DESCRIBE("bitmap_get_nth_element")
  {
    IT("Should get n-th element of a bitmap for n=1..cardinality")
    {
      Bitmap* bitmap = bitmap_alloc();
      uint32_t array[] = { 0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233,
                          377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711,
                          28657, 46368, 75025, 121393, 196418, 317811 };
      size_t array_len = sizeof(array) / sizeof(*array);
      for (size_t i = 0; i < array_len; i++) {
        bitmap_setbit(bitmap, array[i], 1);
      }

      {
        int64_t element = bitmap_get_nth_element_present(bitmap, 0);
        ASSERT(element == -1, "expect first element to return -1");
      }
      {
        for (size_t i = 0; i < array_len; i++) {
          int64_t element = bitmap_get_nth_element_present(bitmap, i + 1);
          ASSERT(element == array[i], "expect item[%zu] to be %llu. Received: %u", i, element, array[i]);
        }
      }
      {
        int64_t element = bitmap_get_nth_element_present(bitmap, array_len + 1);
        ASSERT(element == -1, "expect last element to return -1");
      }

      bitmap_free(bitmap);
    }

    IT("Should get n-th non existent element of a bitmap for n=1..cardinality")
    {
      Bitmap* bitmap = bitmap_alloc();
      uint32_t array[] = { 0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233,
                          377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711,
                          28657, 46368, 75025, 121393, 196418, 317811 };
      size_t array_len = sizeof(array) / sizeof(*array);
      for (size_t i = 0; i < array_len; i++) {
        bitmap_setbit(bitmap, array[i], 1);
      }

      {
        int64_t element = bitmap_get_nth_element_not_present(bitmap, 0);
        ASSERT(element == -1, "expect first element to return -1");
      }
      {
        for (size_t i = 0; i < 1000; i++) {
          int64_t element = bitmap_get_nth_element_not_present(bitmap, i + 1);
          int64_t element_check = bitmap_get_nth_element_not_present_slow(bitmap, i + 1);
          ASSERT(element == element_check, "expect item[%zu] to be %llu. Received: %u", i, element, element_check);
        }
      }
      {
        int64_t element = bitmap_get_nth_element_not_present(bitmap, array[array_len - 1]);
        ASSERT(element == -1, "expect last element to return -1");
      }

      bitmap_free(bitmap);
    }
  }
}
