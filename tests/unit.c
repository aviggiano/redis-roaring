#include <stdio.h>
#include <assert.h>
#include "data-structure.h"
#include "roaring.h"
#include "test-utils.h"
#include "unit/test_bitmap_free.c"
#include "unit/test_bitmap_from_bit_array.c"
#include "unit/test_bitmap_from_int_array.c"
#include "unit/test_bitmap_range_int_array.c"
#include "unit/test_bitmap64_range_int_array.c"
#include "unit/test_bitmap_get_nth_element.c"
#include "unit/test_bitmap_not.c"
#include "unit/test_bitmap_xor.c"
#include "unit/test_bitmap_and.c"
#include "unit/test_bitmap_or.c"
#include "unit/test_bitmap_setbit.c"
#include "unit/test_bitmap64_free.c"
#include "unit/test_bitmap64_or.c"
#include "unit/test_bitmap64_and.c"
#include "unit/test_bitmap64_get_bit_array.c"
#include "unit/test_bitmap64_get_nth_element.c"
#include "unit/test_bitmap64_from_bit_array.c"
#include "unit/test_bitmap64_from_int_array.c"
#include "unit/test_bitmap64_getbit.c"
#include "unit/test_bitmap64_setbit.c"
#include "unit/test_bitmap64_xor.c"
#include "unit/test_bitmap64_not.c"
#include "unit/test_bitmap_andor.c"
#include "unit/test_bitmap64_andor.c"
#include "unit/test_bitmap_one.c"
#include "unit/test_bitmap64_one.c"
#include "unit/test_bitmap64_getbits.c"
#include "unit/test_bitmap_getbits.c"

int main(int argc, char* argv[]) {
  test_start();

  test_bitmap_free();
  test_bitmap_setbit();
  test_bitmap_getbits();
  test_bitmap_from_bit_array();
  test_bitmap_from_int_array();
  test_bitmap64_free();
  test_bitmap64_or();
  test_bitmap64_and();
  test_bitmap64_not();
  test_bitmap64_xor();
  test_bitmap64_range_int_array();
  test_bitmap64_getbit();
  test_bitmap64_getbits();
  test_bitmap64_setbit();
  test_bitmap64_get_bit_array();
  test_bitmap64_from_bit_array();
  test_bitmap64_from_int_array();
  test_bitmap64_get_nth_element();
  test_bitmap_get_nth_element();
  test_bitmap_not();
  test_bitmap_xor();
  test_bitmap_and();
  test_bitmap_or();
  test_bitmap_andor();
  test_bitmap64_andor();
  test_bitmap64_one();
  test_bitmap_one();
  test_bitmap_range_int_array();

  test_end();

  return test_total_failed > 0 ? 1 : 0;
}
