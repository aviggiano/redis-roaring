#include "data-structure.h"
#include "../test-utils.h"

void test_bitmap64_range_int_array() {
  DESCRIBE("bitmap64_range_int_array")
  {
    IT("Basic range extraction from bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t test_values[] = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
      size_t test_values_len = ARRAY_LENGTH(test_values);

      // Add values to bitmap
      for (size_t i = 0; i < test_values_len; i++) {
        roaring64_bitmap_add(bitmap, test_values[i]);
      }

      // Test extracting from offset 0, n=5
      uint64_t result_len;
      uint64_t* result = bitmap64_range_int_array(bitmap, 0, 5, &result_len);
      ASSERT_NOT_NULL(result);

      // Verify results using array comparison
      ASSERT_ARRAY_EQ(test_values, result, 5, 5);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Range extraction with offset")
    {
      Bitmap64* bitmap = roaring64_bitmap_from(100, 200, 300, 400, 500, 600, 700, 800);

      // Extract 3 values starting from offset 2
      uint64_t result_len;
      uint64_t* result = bitmap64_range_int_array(bitmap, 2, 3, &result_len);
      ASSERT_NOT_NULL(result);

      // Create expected array for comparison
      uint64_t expected[] = { 300, 400 };
      ASSERT_ARRAY_EQ(expected, result, 2, result_len);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Large 64-bit value handling")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t large_values[] = {
          UINT64_C(0xFFFFFFFF00000000),
          UINT64_C(0xFFFFFFFF00000001),
          UINT64_C(0xFFFFFFFF00000002),
          UINT64_C(0xFFFFFFFF00000003)
      };
      size_t large_values_len = ARRAY_LENGTH(large_values);

      for (size_t i = 0; i < large_values_len; i++) {
        roaring64_bitmap_add(bitmap, large_values[i]);
      }

      uint64_t* result = bitmap64_range_int_array(bitmap, 0, 4, NULL);
      ASSERT_NOT_NULL(result);

      // Use verbose comparison to see hex values on failure
      ASSERT_ARRAY_EQ(large_values, result, 4, 4);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Early termination when requesting more than available")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t few_values[] = { 1, 3, 5 };
      size_t few_values_len = ARRAY_LENGTH(few_values);

      for (size_t i = 0; i < few_values_len; i++) {
        roaring64_bitmap_add(bitmap, few_values[i]);
      }

      // Request 10 elements but only 3 are available
      uint64_t result_len;
      uint64_t* result = bitmap64_range_int_array(bitmap, 0, 10, &result_len);
      ASSERT_NOT_NULL(result);

      // Check available values match
      ASSERT_ARRAY_EQ(few_values, result, 3, result_len);

      // Check that remaining elements are zero (from calloc)
      ASSERT_ARRAY_RANGE_ZERO(result, 3, 7);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Empty bitmap handling")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();

      uint64_t result_len;
      uint64_t* result = bitmap64_range_int_array(bitmap, 0, 5, &result_len);
      ASSERT_NOT_NULL(result);
      ASSERT_EQ(0, result_len);

      // All values should be 0 (from calloc)
      ASSERT_ARRAY_RANGE_ZERO(result, 0, 5);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Offset beyond available elements")
    {
      Bitmap64* bitmap = roaring64_bitmap_from(10, 20, 30);

      // Start from offset 5, but we only have 3 elements
      uint64_t* result = bitmap64_range_int_array(bitmap, 5, 3, NULL);
      ASSERT_NULL(result);
      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Zero elements requested")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      roaring64_bitmap_add(bitmap, 42);

      uint64_t* result = bitmap64_range_int_array(bitmap, 0, 0, NULL);
      ASSERT_NOT_NULL(result);

      SAFE_FREE(result);
      roaring64_bitmap_free(bitmap);
    }

    IT("Single element bitmap")
    {
      Bitmap64* bitmap = roaring64_bitmap_create();
      uint64_t single_value = 12345;
      roaring64_bitmap_add(bitmap, single_value);

      uint64_t* result = bitmap64_range_int_array(bitmap, 0, 1, NULL);
      ASSERT_NOT_NULL(result);
      ASSERT_EQ(single_value, result[0]);

      // Test requesting more than available
      uint64_t* result2 = bitmap64_range_int_array(bitmap, 0, 3, NULL);
      ASSERT_NOT_NULL(result2);
      ASSERT_EQ(single_value, result2[0]);
      ASSERT_ARRAY_RANGE_ZERO(result2, 1, 2);

      SAFE_FREE(result);
      SAFE_FREE(result2);
      roaring64_bitmap_free(bitmap);
    }

    IT("Duplicate sequence test")
    {
      uint64_t fibonacci[] = { 0, 1, 1, 2, 3, 5, 8, 13, 21 };
      size_t fib_len = ARRAY_LENGTH(fibonacci);

      Bitmap64* bitmap = roaring64_bitmap_create();

      // Add Fibonacci numbers to bitmap
      for (size_t i = 0; i < fib_len; i++) {
        roaring64_bitmap_add(bitmap, fibonacci[i]);
      }

      // Extract all values
      uint64_t result_len;
      uint64_t* result = bitmap64_range_int_array(bitmap, 0, fib_len, &result_len);
      ASSERT_NOT_NULL(result);

      // Compare with original array (exclude duplicates)
      uint64_t fibonacci_undup[] = { 0, 1, 2, 3, 5, 8, 13, 21 };
      ASSERT_ARRAY_EQ(fibonacci_undup, result, ARRAY_LENGTH(fibonacci_undup), result_len);

      // Test partial extraction from middle
      uint64_t partial_len;
      uint64_t* partial = bitmap64_range_int_array(bitmap, 4, 8, &partial_len);
      ASSERT_NOT_NULL(partial);

      uint64_t expected_partial[] = { 5, 8, 13, 21 };
      ASSERT_ARRAY_EQ(expected_partial, partial, 4, partial_len);

      SAFE_FREE(result);
      SAFE_FREE(partial);
      roaring64_bitmap_free(bitmap);
    }
  }
}
