#!/usr/bin/env bash

set -eu

. "$(dirname "$0")/helper.sh"

ERRORMSG_WRONGTYPE="WRONGTYPE Operation against a key holding the wrong kind of value";

function test_setbit_getbit() {
  print_test_header "test_setbit_getbit"

  rcall_assert "R.SETBIT test_setbit_getbit 0 1" "0" "Set bit at position 0 to 1"
  rcall_assert "R.GETBIT test_setbit_getbit 0" "1" "Get bit at position 0"

  rcall_assert "R.SETBIT test_setbit_getbit 0" "ERR wrong number of arguments for 'R.SETBIT' command" "SETBIT with wrong number of arguments"
  rcall_assert "R.GETBIT key 0 1" "ERR wrong number of arguments for 'R.GETBIT' command" "GETBIT with wrong number of arguments"
}

function test_getbits() {
  print_test_header "test_getbits"

  rcall_assert "R.SETINTARRAY test_getbits 1 2 4 5" "OK" "Set bits 1-5"
  rcall_assert "R.GETBITS test_getbits 1 2 3 4 5" "1\n1\n0\n1\n1" "Get bit at position 1-5"
}

function test_clearbits() {
  print_test_header "test_clearbits"

  rcall_assert "R.SETINTARRAY test_clearbits 1 2 3 4 5" "OK" "Set bits 1-5"
  rcall_assert "R.CLEARBITS test_clearbits 1 2 3" "OK" "Clear bit at position 1 3"
  rcall_assert "R.BITCOUNT test_clearbits" "2" "Validate count"
  rcall_assert "R.CLEARBITS test_clearbits 4 5 COUNT" "2" "Clear bit with count"
}

function test_bitop() {
  print_test_header "test_bitop"

  function wrong_arity() {
    rcall_assert "R.BITOP test_bitop_wrong_arity NOOP srckey1 srckey2" "ERR syntax error" "BITOP with invalid operation"
  }

  function setup() {
    rcall "R.SETBIT test_bitop_4 1 1"
    rcall "R.SETBIT test_bitop_4 2 1"
    rcall "R.SETBIT test_bitop_2 1 1"
  }

  function and() {
    rcall_assert "R.BITOP AND test_bitop_dest_2 test_bitop_2 test_bitop_4" "1" "BITOP AND operation"
    rcall_assert "R.GETBIT test_bitop_dest_2 1" "1" "Check result of AND operation at bit 1"
  }

  function or() {
    rcall_assert "R.BITOP OR test_bitop_dest_6 test_bitop_2 test_bitop_4" "2" "BITOP OR operation"
    rcall_assert "R.GETBIT test_bitop_dest_6 1" "1" "Check result of OR operation at bit 1"
    rcall_assert "R.GETBIT test_bitop_dest_6 2" "1" "Check result of OR operation at bit 2"
  }

  function xor() {
    rcall_assert "R.BITOP XOR test_bitop_dest_4 test_bitop_2 test_bitop_4" "1" "BITOP XOR operation"
    rcall_assert "R.GETBIT test_bitop_dest_4 2" "1" "Check result of XOR operation at bit 2"
  }

  function not() {
    rcall_assert "R.BITOP NOT test_bitop_dest_3 test_bitop_4" "1" "BITOP NOT operation"
    rcall_assert "R.GETBIT test_bitop_dest_3 0" "1" "Check result of NOT operation at bit 0"
    rcall_assert "R.MAX test_bitop_dest_3" "0" "Check max value after NOT operation"

    rcall_assert "R.BITOP NOT test_bitop_dest_3 test_bitop_4 3" "2" "BITOP NOT operation with size parameter"
    rcall_assert "R.GETBIT test_bitop_dest_3 0" "1" "Check result of NOT operation at bit 0 (with size)"
    rcall_assert "R.MAX test_bitop_dest_3" "3" "Check max value after NOT operation (with size)"
  }

  function collision() {
    rcall "R.BITOP AND test_bitop_collision test_bitop_4 test_bitop_2"
    rcall "R.BITOP OR test_bitop_collision test_bitop_4 test_bitop_2"
  }

  setup
  wrong_arity
  and
  or
  xor
  and
  collision
}
function test_bitcount() {
  print_test_header "test_bitcount"

  rcall "R.SETBIT test_bitcount 1 1"
  rcall "R.SETBIT test_bitcount 1 1"
  rcall "R.SETBIT test_bitcount 2 1"
  rcall "R.SETBIT test_bitcount 3 1"
  rcall "R.SETBIT test_bitcount 5 1"
  rcall "R.SETBIT test_bitcount 8 1"
  rcall "R.SETBIT test_bitcount 13 1"

  rcall_assert "R.BITCOUNT test_bitcount" "6" "Count all set bits"
}

function test_bitpos() {
  print_test_header "test_bitpos"

  rcall "R.SETBIT test_bitpos_1 3 1"
  rcall "R.SETBIT test_bitpos_1 6 1"
  rcall "R.SETBIT test_bitpos_1 4 1"
  rcall "R.SETBIT test_bitpos_1 12 1"
  rcall "R.SETBIT test_bitpos_1 10 1"
  rcall_assert "R.BITPOS test_bitpos_1 1" "3" "Find first set bit position"

  rcall "R.SETBIT test_bitpos_0 0 1"
  rcall "R.SETBIT test_bitpos_0 1 1"
  rcall "R.SETBIT test_bitpos_0 2 1"
  rcall "R.SETBIT test_bitpos_0 3 1"
  rcall "R.SETBIT test_bitpos_0 4 1"
  rcall "R.SETBIT test_bitpos_0 6 1"
  rcall_assert "R.BITPOS test_bitpos_0 0" "5" "Find first unset bit position"
}

function test_getintarray_setintarray() {
  print_test_header "test_getintarray_setintarray"

  rcall_assert "R.SETINTARRAY test_getintarray_setintarray 0" "OK" "Set integer array with single value 0"
  rcall_assert "R.GETINTARRAY test_getintarray_setintarray" "0" "Get integer array with single value"

  rcall_assert "R.SETINTARRAY test_getintarray_setintarray 1 10 100 1000 10000 100000" "OK" "Set integer array with multiple values"
  rcall_assert "R.GETINTARRAY test_getintarray_setintarray" "$(echo -e "1\n10\n100\n1000\n10000\n100000")" "Get integer array with multiple values"

  rcall_assert "R.SETINTARRAY test_getintarray_setintarray 2 4 8 16 32 64 128" "OK" "Set integer array with powers of 2"
  rcall_assert "R.GETINTARRAY test_getintarray_setintarray" "$(echo -e "2\n4\n8\n16\n32\n64\n128")" "Get integer array with powers of 2"

  rcall_assert "R.GETINTARRAY test_getintarray_setintarray_empty_key" "" "Get integer array from empty key"
}

function test_getbitarray_setbitarray() {
  print_test_header "test_getbitarray_setbitarray"

  rcall_assert "R.SETBITARRAY test_getbitarray_setbitarray 0" "OK" "Set bit array with single 0"
  rcall_assert "R.GETBITARRAY test_getbitarray_setbitarray" "0" "Get bit array with single 0"

  rcall_assert "R.SETBITARRAY test_getbitarray_setbitarray 0110100100010010011001000" "OK" "Set bit array with binary string"
  rcall_assert "R.GETBITARRAY test_getbitarray_setbitarray" "0110100100010010011001" "Get bit array binary string"
  rcall_assert "R.GETBIT test_getbitarray_setbitarray 0" "0" "Check bit 0 in bit array"
  rcall_assert "R.GETBIT test_getbitarray_setbitarray 1" "1" "Check bit 1 in bit array"
  rcall_assert "R.GETBIT test_getbitarray_setbitarray 24" "0" "Check bit 24 in bit array"
  rcall_assert "R.GETBIT test_getbitarray_setbitarray 21" "1" "Check bit 21 in bit array"

  rcall_assert "R.GETBITARRAY test_getbitarray_setbitarray_empty_key" "" "Get bit array from empty key"
}

function test_appendintarray_deleteintarray() {
  print_test_header "test_appendintarray_deleteintarray"

  rcall_assert "R.APPENDINTARRAY test_appendintarray_deleteintarray 1 2 3" "OK" "Initialize bitmap with values"
  rcall_assert "R.GETINTARRAY test_appendintarray_deleteintarray" "$(echo -e "1\n2\n3")" "Get array after initialization"
  rcall_assert "R.DELETEINTARRAY test_appendintarray_deleteintarray 1 3" "OK" "Delete values 1 and 3"
  rcall_assert "R.GETINTARRAY test_appendintarray_deleteintarray" "2" "Get array after deletion"
}

function test_min_max() {
  print_test_header "test_min_max"

  rcall_assert "R.MIN test_min_max" "-1" "Get min from empty bitmap"
  rcall_assert "R.MAX test_min_max" "-1" "Get max from empty bitmap"

  rcall_assert "R.SETBIT test_min_max 100 1" "0" "Set bit 100"
  rcall_assert "R.MIN test_min_max" "100" "Get min after setting bit 100"
  rcall_assert "R.MAX test_min_max" "100" "Get max after setting bit 100"

  rcall_assert "R.SETBIT test_min_max 0 1" "0" "Set bit 0"
  rcall_assert "R.MIN test_min_max" "0" "Get min after setting bit 0"
  rcall_assert "R.MAX test_min_max" "100" "Get max after setting both bits"

  rcall_assert "R.SETBIT test_min_max 0 0" "1" "Unset bit 0"
  rcall_assert "R.SETBIT test_min_max 100 0" "1" "Unset bit 100"
  rcall_assert "R.MIN test_min_max" "-1" "Get min after unsetting all bits"
  rcall_assert "R.MAX test_min_max" "-1" "Get max after unsetting all bits"
}

function test_bitop_one() {
  print_test_header "test_bitop_one"

  # Test 1: Empty bitmap array
  rcall_assert "R.BITOP ONE test_bitop_one_result_empty" "ERR wrong number of arguments for 'R.BITOP' command" "BITOP ONE with no source bitmaps"

  # Test 2: Single bitmap
  rcall_assert "R.BITOP ONE test_bitop_one_result_single test_bitop_one_key1" "ERR wrong number of arguments for 'R.BITOP' command" "BITOP ONE with single bitmap"

  # Test 3: Two non-overlapping bitmaps
  rcall_assert "R.SETINTARRAY test_bitop_one_key1 1 3 5" "OK" "Set bits in test_bitop_one_key1"
  rcall_assert "R.SETINTARRAY test_bitop_one_key2 2 4 6" "OK" "Set bits in test_bitop_one_key2"

  rcall_assert "R.BITOP ONE test_bitop_one_result_non_overlap test_bitop_one_key1 test_bitop_one_key2" "6" "BITOP ONE with non-overlapping bitmaps"
  rcall_assert "R.GETINTARRAY test_bitop_one_result_non_overlap" "1\n2\n3\n4\n5\n6" "Result should contain all bits from both keys"

  # Test 4: Two overlapping bitmaps
  rcall_assert "R.SETINTARRAY test_bitop_one_key3 1 2 3" "OK" "Set bits in test_bitop_one_key3"
  rcall_assert "R.SETINTARRAY test_bitop_one_key4 3 4 5" "OK" "Set bits in test_bitop_one_key4"

  rcall_assert "R.BITOP ONE test_bitop_one_result_overlap test_bitop_one_key3 test_bitop_one_key4" "4" "BITOP ONE with overlapping bitmaps"
  rcall_assert "R.GETINTARRAY test_bitop_one_result_overlap" "1\n2\n4\n5" "Result should contain only non-overlapping bits"

  # Test 5: Three bitmaps - Redis example
  rcall_assert "R.SETINTARRAY test_bitop_one_key_a 0 4 5 6" "OK" "Set bits in test_bitop_one_key_a"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_b 1 5 6" "OK" "Set bits in test_bitop_one_key_b"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_c 2 3 5 6 7" "OK" "Set bits in test_bitop_one_key_c"
  
  rcall_assert "R.BITOP ONE test_bitop_one_result_three test_bitop_one_key_a test_bitop_one_key_b test_bitop_one_key_c" "6" "BITOP ONE with three bitmaps"
  rcall_assert "R.GETINTARRAY test_bitop_one_result_three" "0\n1\n2\n3\n4\n7" "Result should contain bits appearing exactly once"

  # Test 6: All bitmaps have same bits
  rcall_assert "R.SETINTARRAY test_bitop_one_key_same1 10 20 30" "OK" "Set bits in test_bitop_one_key_same1"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_same2 10 20 30" "OK" "Set bits in test_bitop_one_key_same2"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_same3 10 20 30" "OK" "Set bits in test_bitop_one_key_same3"

  rcall_assert "R.BITOP ONE test_bitop_one_result_same test_bitop_one_key_same1 test_bitop_one_key_same2 test_bitop_one_key_same3" "0" "BITOP ONE with identical bitmaps"
  rcall_assert "R.GETINTARRAY test_bitop_one_result_same" "" "Result should be empty array"

  # Test 7: One empty bitmap among others
  rcall_assert "R.SETINTARRAY test_bitop_one_key_with_bits1 1 2 3" "OK" "Set bits in test_bitop_one_key_with_bits1"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_with_bits2 3 4 5" "OK" "Set bits in test_bitop_one_key_with_bits2"

  rcall "DEL test_bitop_one_key_empty"
  rcall_assert "R.BITOP ONE test_bitop_one_result_mixed test_bitop_one_key_with_bits1 test_bitop_one_key_empty test_bitop_one_key_with_bits2" "4" "BITOP ONE with one empty bitmap"
  rcall_assert "R.GETINTARRAY test_bitop_one_result_mixed" "1\n2\n4\n5" "Result should contain non-overlapping bits"

  # Test 8: Complex overlaps with four bitmaps
  rcall_assert "R.SETINTARRAY test_bitop_one_key_complex1 1 2 3 4 5" "OK" "Set bits in test_bitop_one_key_complex1"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_complex2 2 3 4 6 7" "OK" "Set bits in test_bitop_one_key_complex2"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_complex3 3 4 5 7 8" "OK" "Set bits in test_bitop_one_key_complex3"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_complex4 4 5 6 8 9" "OK" "Set bits in test_bitop_one_key_complex4"

  rcall_assert "R.BITOP ONE test_bitop_one_result_complex test_bitop_one_key_complex1 test_bitop_one_key_complex2 test_bitop_one_key_complex3 test_bitop_one_key_complex4" "2" "BITOP ONE with complex overlaps"
  rcall_assert "R.GETINTARRAY test_bitop_one_result_complex" "1\n9" "Result should contain only bits appearing exactly once"

  # Test 9: Large bit values
  rcall_assert "R.SETINTARRAY test_bitop_one_key_large1 1000000 2000000" "OK" "Set large bits in key_large1"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_large2 2000000 3000000" "OK" "Set large bits in key_large2"

  rcall_assert "R.BITOP ONE test_bitop_one_result_large test_bitop_one_key_large1 test_bitop_one_key_large2" "2" "BITOP ONE with large bit values"
  rcall_assert "R.GETINTARRAY test_bitop_one_result_large" "1000000\n3000000" "Result should contain non-overlapping large bits"

  # Test 10: Destination already has content
  rcall_assert "R.SETINTARRAY test_bitop_one_result_preexist 100 200 300" "OK" "Set bits in test_bitop_one_result_preexist"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_overwrite1 1 2" "OK" "Set bits in test_bitop_one_key_overwrite1"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_overwrite2 2 3" "OK" "Set bits in test_bitop_one_key_overwrite2"

  rcall_assert "R.BITOP ONE test_bitop_one_result_preexist test_bitop_one_key_overwrite1 test_bitop_one_key_overwrite2" "2" "BITOP ONE should overwrite destination"
  rcall_assert "R.GETINTARRAY test_bitop_one_result_preexist" "1\n3" "Result should only contain new operation result"

  # Test 11: Input bitmaps should remain unchanged
  rcall_assert "R.SETINTARRAY test_bitop_one_key_immutable1 10 20 30" "OK" "Set bits in test_bitop_one_key_immutable1"
  rcall_assert "R.SETINTARRAY test_bitop_one_key_immutable2 20 30 40" "OK" "Set bits in test_bitop_one_key_immutable2"

  rcall_assert "R.BITOP ONE test_bitop_one_result_immutable test_bitop_one_key_immutable1 test_bitop_one_key_immutable2" "2" "BITOP ONE operation"
  rcall_assert "R.GETINTARRAY test_bitop_one_key_immutable1" "10\n20\n30" "key_immutable1 should remain unchanged"
  rcall_assert "R.GETINTARRAY test_bitop_one_key_immutable2" "20\n30\n40" "key_immutable2 should remain unchanged"
  rcall_assert "R.GETINTARRAY test_bitop_one_result_immutable" "10\n40" "Result should contain bits appearing exactly once"
}

function test_diff() {
  print_test_header "test_diff"

  rcall_assert "R.DIFF test_diff_res" "ERR wrong number of arguments for 'R.DIFF' command" "DIFF with wrong number of arguments"
  rcall_assert "R.DIFF test_diff_res empty_key_1 empty_key_2" "${ERRORMSG_WRONGTYPE}" "DIFF with empty keys"

  rcall_assert "R.SETINTARRAY test_diff_1 1 2 3 4" "OK" "Set first array for diff test"
  rcall_assert "R.SETINTARRAY test_diff_2 3 4 5 6" "OK" "Set second array for diff test"

  rcall_assert "R.DIFF test_diff_res test_diff_1 test_diff_2" "OK" "Compute difference test_diff_1 - test_diff_2"
  rcall_assert "R.GETINTARRAY test_diff_res" "$(echo -e "1\n2")" "Get result of first difference"

  rcall_assert "R.DIFF test_diff_res test_diff_2 test_diff_1" "OK" "Compute difference test_diff_2 - test_diff_1"
  rcall_assert "R.GETINTARRAY test_diff_res" "$(echo -e "5\n6")" "Get result of second difference"
}

function test_setrage() {
  print_test_header "test_setrage"

  rcall_assert "R.SETRANGE test_setrange 0 5" "OK" "Should set range from 0 to 5"
  rcall_assert "R.GETINTARRAY test_setrange" "$(echo -e "0\n1\n2\n3\n4")" "Validate range from 0 5"
  rcall_assert "R.SETRANGE test_setrange 5 5" "OK" "Should handle zero range"
}

function test_clear() {
  print_test_header "test_clear"

  rcall_assert "R.SETINTARRAY test_clear 1 2 3 4 5" "OK" "Set bits for deletion test"
  rcall_assert "R.CLEAR test_clear" "5" "Clear key test_clear"
  rcall_assert "R.BITCOUNT test_clear" "0" "Should be empty after clear"
  rcall_assert "R.CLEAR test_clear_not_exists" "" "Should return nil for non exist key"
}

function test_optimize_nokey() {
  print_test_header "test_optimize nokey"
  rcall_assert "R.OPTIMIZE no-key" "ERR no such key" "Optimize non-existent key"
}

function test_setfull() {
  print_test_header "test_setfull"

  rcall "del foo"
  rcall "R.SETFULL foo"
  rcall_assert "R.BITCOUNT foo" "4294967296" "Count bits in full bitmap"
}

function test_del() {
  print_test_header "test_del"

  rcall_assert "R.SETBIT test_del 0 1" "0" "Set bit for deletion test"
  rcall_assert "DEL test_del" "1" "Delete key test_del"
}

function test_contains() {
  print_test_header "test_contains"

  # Setup test data
  rcall_assert "R.SETINTARRAY test_contains1 1 2 3 4 5" "OK" "Setup test_contains1 with [1,2,3,4,5]"
  rcall_assert "R.SETINTARRAY test_contains2 2 3" "OK" "Setup test_contains2 with [2,3]"
  rcall_assert "R.SETINTARRAY test_contains3 3 4 6" "OK" "Setup test_contains3 with [3,4,6]"
  rcall_assert "R.SETINTARRAY test_contains4 1 2 3 4 5" "OK" "Setup test_contains4 with [1,2,3,4,5] (same as test_contains1)"
  rcall_assert "R.SETBIT test_contains5 0 1" "0" "Setup empty test_contains5"
  rcall_assert "R.CLEAR test_contains5" "1" "Cleanup test_contains5"
  rcall_assert "R.SETINTARRAY test_contains6 7 8 9" "OK" "Setup test_contains6 with [7,8,9] (no intersection with test_contains1)"
  rcall_assert "R.SETINTARRAY test_contains_eq1 1 2 3 4 5" "OK" "Setup eq1 with [1,2,3,4,5]"
  rcall_assert "R.SETINTARRAY test_contains_eq2 1 2 3 4 5" "OK" "Setup eq2 with [1,2,3,4,5] (same as eq1)"

  # Test basic intersection (default mode - should be NONE)
  rcall_assert "R.CONTAINS test_contains1 test_contains2" "1" "test_contains1 contains some elements from test_contains2 (intersection exists)"
  rcall_assert "R.CONTAINS test_contains1 test_contains6" "0" "test_contains1 doesn't intersect with test_contains6"
  rcall_assert "R.CONTAINS test_contains5 test_contains1" "0" "Empty test_contains5 doesn't intersect with test_contains1"
  rcall_assert "R.CONTAINS test_contains1 test_contains5" "0" "test_contains1 doesn't intersect with empty test_contains5"

  # Test ALL mode (test_contains2 is subset of test_contains1)
  rcall_assert "R.CONTAINS test_contains1 test_contains2 ALL" "1" "test_contains2 [2,3] is subset of test_contains1 [1,2,3,4,5]"
  rcall_assert "R.CONTAINS test_contains1 test_contains3 ALL" "0" "test_contains3 [3,4,6] is not subset of test_contains1 (6 not in test_contains1)"
  rcall_assert "R.CONTAINS test_contains1 test_contains4 ALL" "1" "test_contains4 [1,2,3,4,5] is subset of test_contains1 [1,2,3,4,5] (equal sets)"
  rcall_assert "R.CONTAINS test_contains1 test_contains5 ALL" "1" "Empty test_contains5 is subset of any bitmap"
  rcall_assert "R.CONTAINS test_contains5 test_contains1 ALL" "0" "Non-empty test_contains1 is not subset of empty test_contains5"

  # Test ALL_STRICT mode (test_contains2 is strict subset of test_contains1)
  rcall_assert "R.CONTAINS test_contains1 test_contains2 ALL_STRICT" "1" "test_contains2 [2,3] is strict subset of test_contains1 [1,2,3,4,5]"
  rcall_assert "R.CONTAINS test_contains1 test_contains3 ALL_STRICT" "0" "test_contains3 [3,4,6] is not subset of test_contains1"
  rcall_assert "R.CONTAINS test_contains1 test_contains4 ALL_STRICT" "0" "test_contains4 [1,2,3,4,5] is not strict subset of test_contains1 (equal sets)"
  rcall_assert "R.CONTAINS test_contains1 test_contains5 ALL_STRICT" "1" "Empty test_contains5 is strict subset of non-empty test_contains1"
  rcall_assert "R.CONTAINS test_contains5 test_contains1 ALL_STRICT" "0" "Non-empty test_contains1 is not strict subset of empty test_contains5"
  rcall_assert "R.CONTAINS test_contains5 test_contains5 ALL_STRICT" "0" "Empty bitmap is not strict subset of itself"

  # Test EQ mode identical bitmaps
  rcall_assert "R.CONTAINS test_contains_eq1 test_contains_eq2 EQ" "1" "test_contains_eq1 and test_contains_eq2 are equal"
  rcall_assert "R.CONTAINS test_contains_eq2 test_contains_eq1 EQ" "1" "test_contains_eq2 and test_contains_eq1 are equal (symmetry test)"

  # Test EQ mode non-equal bitmaps
  rcall_assert "R.CONTAINS test_contains_eq1 test_contains6 EQ" "0" "test_contains_eq1 [1,2,3,4,5] is not equal to test_contains6"
  rcall_assert "R.CONTAINS test_contains_eq1 test_contains5 EQ" "0" "test_contains_eq1 [1,2,3,4,5] is not equal to test_contains5"

  # Test with single element bitmaps
  rcall_assert "R.SETINTARRAY test_contains_single1 3" "OK" "Setup single element bitmap with [3]"
  rcall_assert "R.SETINTARRAY test_contains_single2 7" "OK" "Setup single element bitmap with [7]"
  
  rcall_assert "R.CONTAINS test_contains1 test_contains_single1" "1" "test_contains1 intersects with test_contains_single1 [3]"
  rcall_assert "R.CONTAINS test_contains1 test_contains_single2" "0" "test_contains1 doesn't intersect with test_contains_single2 [7]"
  rcall_assert "R.CONTAINS test_contains1 test_contains_single1 ALL" "1" "test_contains_single1 [3] is subset of test_contains1"
  rcall_assert "R.CONTAINS test_contains1 test_contains_single2 ALL" "0" "test_contains_single2 [7] is not subset of test_contains1"
  rcall_assert "R.CONTAINS test_contains1 test_contains_single1 ALL_STRICT" "1" "test_contains_single1 [3] is strict subset of test_contains1"

  # Test error cases
  rcall_assert "R.CONTAINS nonexistent test_contains1" "${ERRORMSG_WRONGTYPE}" "Should return error for non-existent first key"
  rcall_assert "R.CONTAINS test_contains1 nonexistent" "${ERRORMSG_WRONGTYPE}" "Should return error for non-existent second key"
  rcall_assert "R.CONTAINS nonexistent1 nonexistent2" "${ERRORMSG_WRONGTYPE}" "Should return error when both keys don't exist"
  
  # Test invalid mode
  rcall_assert "R.CONTAINS test_contains1 test_contains2 INVALID_MODE" "ERR invalid mode argument: INVALID_MODE" "Should return error for invalid mode"
  rcall_assert "R.CONTAINS test_contains1 test_contains2 all" "ERR invalid mode argument: all" "Should return error for lowercase mode (case sensitive)"

  # Test with larger datasets
  rcall_assert "R.SETINTARRAY test_contains_large1 $(seq 1 1000 | tr '\n' ' ')" "OK" "Setup large test_contains1 with 1-1000"
  rcall_assert "R.SETINTARRAY test_contains_large2 $(seq 100 200 | tr '\n' ' ')" "OK" "Setup large test_contains2 with 100-200"
  rcall_assert "R.SETINTARRAY test_contains_large3 $(seq 1001 1100 | tr '\n' ' ')" "OK" "Setup large test_contains3 with 1001-1100"
  
  rcall_assert "R.CONTAINS test_contains_large1 test_contains_large2" "1" "Large bitmaps intersection test"
  rcall_assert "R.CONTAINS test_contains_large1 test_contains_large3" "0" "Large bitmaps no intersection test"
  rcall_assert "R.CONTAINS test_contains_large1 test_contains_large2 ALL" "1" "Large bitmap subset test"
  rcall_assert "R.CONTAINS test_contains_large1 test_contains_large3 ALL" "0" "Large bitmap not subset test"
  rcall_assert "R.CONTAINS test_contains_large1 test_contains_large2 ALL_STRICT" "1" "Large bitmap strict subset test"

  # Test with ranges
  rcall_assert "R.SETINTARRAY test_contains_range1 1 5 10 15 20" "OK" "Setup sparse range bitmap"
  rcall_assert "R.SETINTARRAY test_contains_range2 5 15" "OK" "Setup subset range bitmap"
  rcall_assert "R.SETINTARRAY test_contains_range3 5 25" "OK" "Setup partial overlap range bitmap"
  
  rcall_assert "R.CONTAINS test_contains_range1 test_contains_range2" "1" "Sparse range intersection test"
  rcall_assert "R.CONTAINS test_contains_range1 test_contains_range3" "1" "Partial overlap intersection test"
  rcall_assert "R.CONTAINS test_contains_range1 test_contains_range2 ALL" "1" "Sparse range subset test"
  rcall_assert "R.CONTAINS test_contains_range1 test_contains_range3 ALL" "0" "Partial overlap not subset test"
  rcall_assert "R.CONTAINS test_contains_range1 test_contains_range2 ALL_STRICT" "1" "Sparse range strict subset test"
}

function test_stat() {
  print_test_header "test_stat"

  rcall_assert "R.SETBIT test_stat 100 1" "0" "Set bit 100 for stat test"

  EXPECTED_STAT=$'type: bitmap\ncardinality: 1\nnumber of containers: 1\nmax value: 100\nmin value: 100
number of array containers: 1\n\tarray container values: 1\n\tarray container bytes: 2
bitset  containers: 0\n\tbitset  container values: 0\n\tbitset  container bytes: 0
run containers: 0\n\trun container values: 0\n\trun container bytes: 0'
  
  rcall_assert "R.STAT test_stat" "$EXPECTED_STAT" "Get bitmap statistics"

  EXPECTED_STAT=$'{"type":"bitmap","cardinality":"1","number_of_containers":"1","max_value":"100","min_value":"100","array_container":{"number_of_containers":"1","container_cardinality":"1","container_allocated_bytes":"2"},"bitset_container":{"number_of_containers":"0","container_cardinality":"0","container_allocated_bytes":"0"},"run_container":{"number_of_containers":"0","container_cardinality":"0","container_allocated_bytes":"0"}}'

  rcall_assert "R.STAT test_stat JSON" "$EXPECTED_STAT" "Get bitmap statistics (json)"
}

function test_save() {
  print_test_header "test_save"
  rcall_assert "SAVE" "OK" "Save Redis database"
}

test_setbit_getbit
test_getbits
test_clearbits
test_bitop
test_bitcount
test_bitpos
test_getintarray_setintarray
test_getbitarray_setbitarray
test_appendintarray_deleteintarray
test_min_max
test_bitop_one
test_setrage
test_clear
test_diff
test_optimize_nokey
test_setfull
test_del
test_contains
test_stat
test_save
