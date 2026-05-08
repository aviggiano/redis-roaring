#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_get_nth_element() {
  DESCRIBE("bitmap64_get_nth_element")
  {
    IT("Should get n-th element of a bitmap for n=1..cardinality")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t array[] = { 0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233,
                          377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711,
                          28657, 46368, 75025, 121393, 196418, 317811 };
      size_t array_len = ARRAY_LENGTH(array);
      for (size_t i = 0; i < ARRAY_LENGTH(array); i++) {
        bitmap64_setbit(bitmap, array[i], 1);
      }

      {
        bool found = false;
        uint64_t element = bitmap64_get_nth_element_present(bitmap, 0, &found);
        ASSERT(element == 0, "expect first element to return 0");
        ASSERT(found == false, "expect first element to found output false");
      }
      {
        for (size_t i = 0; i < array_len; i++) {
          bool found = false;
          uint64_t element = bitmap64_get_nth_element_present(bitmap, i + 1, &found);
          ASSERT(element == array[i], "expect item[%zu] to be %llu. Received: %u, FoundFlag = %d", i, element, array[i], found);
        }
      }
      {
        bool found = false;
        uint64_t element = bitmap64_get_nth_element_present(bitmap, array_len + 1, &found);
        ASSERT(element == 0, "expect last element to return 0");
        ASSERT(found == false, "expect first element to found output false");
      }

      bitmap64_free(bitmap);
    }

    IT("Should get n-th non existent element of a bitmap for n=1..cardinality")
    {
      Bitmap64* bitmap = bitmap64_alloc();
      uint64_t array[] = { 0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233,
                          377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711,
                          28657, 46368, 75025, 121393, 196418, 317811 };
      size_t array_len = ARRAY_LENGTH(array);
      for (size_t i = 0; i < array_len; i++) {
        bitmap64_setbit(bitmap, array[i], 1);
      }

      {
        bool found = false;
        uint64_t element = bitmap64_get_nth_element_not_present(bitmap, 0, &found);
        ASSERT(element == 0, "expect first item to return 0");
        ASSERT(found == false, "expect first item found = false");
      }
      {
        for (size_t i = 0; i < 1000; i++) {
          bool found = false;
          bool slow_found = false;
          uint64_t element = bitmap64_get_nth_element_not_present(bitmap, i + 1, &found);
          uint64_t element_check = bitmap64_get_nth_element_not_present_slow(bitmap, i + 1, &slow_found);
          ASSERT(element == element_check, "expect item[%zu] to be %llu. Received: %u", i, element, element_check);
          ASSERT(found == slow_found, "expect found flag for item[%zu] to match slow path", i);
        }
      }
      {
        Bitmap64* single = bitmap64_alloc();
        bool found = false;
        bitmap64_setbit(single, 0, 1);
        ASSERT(bitmap64_get_nth_element_not_present(single, 1, &found) == 1, "expect first missing element after {0} to be 1");
        ASSERT(found == true, "expect first missing element after {0} to be found");
        ASSERT(bitmap64_get_nth_element_not_present(single, 2, &found) == 2, "expect second missing element after {0} to be 2");
        ASSERT(found == true, "expect second missing element after {0} to be found");
        bitmap64_free(single);
      }

      bitmap64_free(bitmap);
    }
  }
}
