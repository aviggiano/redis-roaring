#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_get_bit_array() {

  DESCRIBE("bitmap64_get_bit_array")
  {
    IT("Should handle empty bitmap")
    {
      // Create empty bitmap
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t size;

      char* result = bitmap64_get_bit_array(bitmap, &size);

      ASSERT_NOT_NULL(result);
      ASSERT_EQ(1, size); // size should be 1 for empty bitmap (max + 1 = 0 + 1)
      ASSERT_EQ_STR("0", result);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle single bit set at position 0")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      roaring64_bitmap_add(bitmap, 0);
      uint64_t size;

      char* result = bitmap64_get_bit_array(bitmap, &size);

      ASSERT_NOT_NULL(result);
      ASSERT_EQ(1, size);
      ASSERT_EQ_STR("1", result);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle single bit set at higher position")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      roaring64_bitmap_add(bitmap, 5);
      uint64_t size;

      char* result = bitmap64_get_bit_array(bitmap, &size);

      ASSERT_NOT_NULL(result);
      ASSERT_EQ(6, size); // positions 0-5, so size = 6
      ASSERT_EQ_STR("000001", result);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle multiple consecutive bits")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      roaring64_bitmap_add(bitmap, 0);
      roaring64_bitmap_add(bitmap, 1);
      roaring64_bitmap_add(bitmap, 2);
      uint64_t size;

      char* result = bitmap64_get_bit_array(bitmap, &size);

      ASSERT_NOT_NULL(result);
      ASSERT_EQ(3, size);
      ASSERT_EQ_STR("111", result);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle multiple non-consecutive bits")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      roaring64_bitmap_add(bitmap, 0);
      roaring64_bitmap_add(bitmap, 2);
      roaring64_bitmap_add(bitmap, 7);
      uint64_t size;

      char* result = bitmap64_get_bit_array(bitmap, &size);

      ASSERT_NOT_NULL(result);
      ASSERT_EQ(8, size);
      ASSERT_EQ_STR("10100001", result);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle large bit positions")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      roaring64_bitmap_add(bitmap, 1000);
      roaring64_bitmap_add(bitmap, 1005);
      uint64_t size;

      char* result = bitmap64_get_bit_array(bitmap, &size);

      ASSERT_NOT_NULL(result);
      ASSERT_EQ(1006, size); // maximum is 1005, so size = 1006
      ASSERT_EQ('1', result[1000]);
      ASSERT_EQ('1', result[1005]);
      ASSERT_EQ('0', result[999]);
      ASSERT_EQ('0', result[1001]);
      ASSERT_EQ('\0', result[1006]); // null terminator

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should properly null-terminate the result string")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      roaring64_bitmap_add(bitmap, 3);
      uint64_t size;

      char* result = bitmap64_get_bit_array(bitmap, &size);

      ASSERT_NOT_NULL(result);
      ASSERT_EQ(4, size);
      ASSERT_EQ('\0', result[size]); // check null terminator at correct position
      ASSERT_EQ(4, strlen(result)); // string length should match size

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should handle bitmap with maximum value only")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t max_val = 10;
      roaring64_bitmap_add(bitmap, max_val);
      uint64_t size;

      char* result = bitmap64_get_bit_array(bitmap, &size);

      ASSERT_NOT_NULL(result);
      ASSERT_EQ(max_val + 1, size);
      ASSERT_EQ('1', result[max_val]);
      ASSERT_EQ('0', result[0]);
      ASSERT_EQ('0', result[max_val - 1]);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Should correctly set size parameter")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      roaring64_bitmap_add(bitmap, 0);
      roaring64_bitmap_add(bitmap, 50);
      uint64_t size = 999; // Initialize with garbage value

      char* result = bitmap64_get_bit_array(bitmap, &size);

      ASSERT_NOT_NULL(result);
      ASSERT_EQ(51, size); // Should be set to maximum + 1 = 50 + 1 = 51

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }
  }
}
