#!/usr/bin/env bash

set -eu

. "$(dirname "$0")/helper.sh"

ERRORMSG_WRONGTYPE="WRONGTYPE Operation against a key holding the wrong kind of value"
ERRORMSG_KEY_MISSED="Roaring: key does not exist"
ERRORMSG_KEY_EXISTS="Roaring: key already exist"
ERRORMSG_SET_VALUE="Roaring: error setting value"

function test_setbit_getbit() {
  print_test_header "test_setbit_getbit (64)"

  rcall_assert "R64.SETBIT test_setbit_getbit 0 1" "0" "Set bit at position 0 to 1"
  rcall_assert "R64.GETBIT test_setbit_getbit 0" "1" "Get bit at position 0"

  rcall_assert "R64.SETBIT test_setbit_getbit 0" "ERR wrong number of arguments for 'R64.SETBIT' command" "SETBIT with wrong number of arguments"
  rcall_assert "R64.GETBIT key 0 1" "ERR wrong number of arguments for 'R64.GETBIT' command" "GETBIT with wrong number of arguments"
}

function test_getbits() {
  print_test_header "test_getbits (64)"

  rcall_assert "R64.SETINTARRAY test_getbits 1 2 4 5" "OK" "Set bits 1-5"
  rcall_assert "R64.GETBITS test_getbits 1 2 3 4 5" "1\n1\n0\n1\n1" "Get bit at position 1-5"
}

function test_clearbits() {
  print_test_header "test_clearbits (64)"

  rcall_assert "R64.SETINTARRAY test_clearbits 1 2 3 4 5" "OK" "Set bits 1-5"
  rcall_assert "R64.CLEARBITS test_clearbits 1 2 3" "OK" "Clear bit at position 1 3"
  rcall_assert "R64.BITCOUNT test_clearbits" "2" "Validate count"
  rcall_assert "R64.CLEARBITS test_clearbits 4 5 COUNT" "2" "Clear bit with count"
}

function test_bitop() {
  print_test_header "test_bitop (64)"

  function wrong_arity() {
    rcall_assert "R64.BITOP test_bitop_wrong_arity NOOP srckey1 srckey2" "ERR syntax error" "BITOP with invalid operation"
  }

  function setup() {
    rcall "R64.SETBIT test_bitop_4 1 1"
    rcall "R64.SETBIT test_bitop_4 2 1"
    rcall "R64.SETBIT test_bitop_2 1 1"
  }

  function and() {
    rcall_assert "R64.BITOP AND test_bitop_dest_2 test_bitop_2 test_bitop_4" "1" "BITOP AND operation"
    rcall_assert "R64.GETBIT test_bitop_dest_2 1" "1" "Check result of AND operation at bit 1"
  }

  function or() {
    rcall_assert "R64.BITOP OR test_bitop_dest_6 test_bitop_2 test_bitop_4" "2" "BITOP OR operation"
    rcall_assert "R64.GETBIT test_bitop_dest_6 1" "1" "Check result of OR operation at bit 1"
    rcall_assert "R64.GETBIT test_bitop_dest_6 2" "1" "Check result of OR operation at bit 2"
  }

  function xor() {
    rcall_assert "R64.BITOP XOR test_bitop_dest_4 test_bitop_2 test_bitop_4" "1" "BITOP XOR operation"
    rcall_assert "R64.GETBIT test_bitop_dest_4 2" "1" "Check result of XOR operation at bit 2"
  }

  function not() {
    rcall_assert "R64.BITOP NOT test_bitop_dest_3 test_bitop_4" "1" "BITOP NOT operation"
    rcall_assert "R64.GETBIT test_bitop_dest_3 0" "1" "Check result of NOT operation at bit 0"
    rcall_assert "R64.MAX test_bitop_dest_3" "0" "Check max value after NOT operation"

    rcall_assert "R64.BITOP NOT test_bitop_dest_3 test_bitop_4 3" "2" "BITOP NOT operation with size parameter"
    rcall_assert "R64.GETBIT test_bitop_dest_3 0" "1" "Check result of NOT operation at bit 0 (with size)"
    rcall_assert "R64.MAX test_bitop_dest_3" "3" "Check max value after NOT operation (with size)"
  }

  function collision() {
    rcall "R64.BITOP AND test_bitop_collision test_bitop_4 test_bitop_2"
    rcall "R64.BITOP OR test_bitop_collision test_bitop_4 test_bitop_2"
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
  print_test_header "test_bitcount (64)"

  rcall "R64.SETBIT test_bitcount 1 1"
  rcall "R64.SETBIT test_bitcount 1 1"
  rcall "R64.SETBIT test_bitcount 2 1"
  rcall "R64.SETBIT test_bitcount 3 1"
  rcall "R64.SETBIT test_bitcount 5 1"
  rcall "R64.SETBIT test_bitcount 8 1"
  rcall "R64.SETBIT test_bitcount 13 1"

  rcall_assert "R64.BITCOUNT test_bitcount" "6" "Count all set bits"
}

function test_bitpos() {
  print_test_header "test_bitpos (64)"

  rcall "R64.SETBIT test_bitpos_1 3 1"
  rcall "R64.SETBIT test_bitpos_1 6 1"
  rcall "R64.SETBIT test_bitpos_1 4 1"
  rcall "R64.SETBIT test_bitpos_1 12 1"
  rcall "R64.SETBIT test_bitpos_1 10 1"
  rcall_assert "R64.BITPOS test_bitpos_1 1" "3" "Find first set bit position"

  rcall "R64.SETBIT test_bitpos_0 0 1"
  rcall "R64.SETBIT test_bitpos_0 1 1"
  rcall "R64.SETBIT test_bitpos_0 2 1"
  rcall "R64.SETBIT test_bitpos_0 3 1"
  rcall "R64.SETBIT test_bitpos_0 4 1"
  rcall "R64.SETBIT test_bitpos_0 6 1"
  rcall_assert "R64.BITPOS test_bitpos_0 0" "5" "Find first unset bit position"
}

function test_getintarray_setintarray() {
  print_test_header "test_getintarray_setintarray (64)"

  rcall_assert "R64.SETINTARRAY test_getintarray_setintarray 0" "OK" "Set integer array with single value 0"
  rcall_assert "R64.GETINTARRAY test_getintarray_setintarray" "0" "Get integer array with single value"

  rcall_assert "R64.SETINTARRAY test_getintarray_setintarray 1 10 100 1000 10000 100000" "OK" "Set integer array with multiple values"
  rcall_assert "R64.GETINTARRAY test_getintarray_setintarray" "$(echo -e "1\n10\n100\n1000\n10000\n100000")" "Get integer array with multiple values"

  rcall_assert "R64.SETINTARRAY test_getintarray_setintarray 2 4 8 16 32 64 128" "OK" "Set integer array with powers of 2"
  rcall_assert "R64.GETINTARRAY test_getintarray_setintarray" "$(echo -e "2\n4\n8\n16\n32\n64\n128")" "Get integer array with powers of 2"

  rcall_assert "R64.GETINTARRAY test_getintarray_setintarray_empty_key" "" "Get integer array from empty key"
}

function test_getbitarray_setbitarray() {
  print_test_header "test_getbitarray_setbitarray (64)"

  rcall_assert "R64.SETBITARRAY test_getbitarray_setbitarray 0" "OK" "Set bit array with single 0"
  rcall_assert "R64.GETBITARRAY test_getbitarray_setbitarray" "0" "Get bit array with single 0"

  rcall_assert "R64.SETBITARRAY test_getbitarray_setbitarray 0110100100010010011001000" "OK" "Set bit array with binary string"
  rcall_assert "R64.GETBITARRAY test_getbitarray_setbitarray" "0110100100010010011001" "Get bit array binary string"
  rcall_assert "R64.GETBIT test_getbitarray_setbitarray 0" "0" "Check bit 0 in bit array"
  rcall_assert "R64.GETBIT test_getbitarray_setbitarray 1" "1" "Check bit 1 in bit array"
  rcall_assert "R64.GETBIT test_getbitarray_setbitarray 24" "0" "Check bit 24 in bit array"
  rcall_assert "R64.GETBIT test_getbitarray_setbitarray 21" "1" "Check bit 21 in bit array"

  rcall_assert "R64.GETBITARRAY test_getbitarray_setbitarray_empty_key" "" "Get bit array from empty key"
}

function test_appendintarray_deleteintarray() {
  print_test_header "test_appendintarray_deleteintarray (64)"

  rcall_assert "R64.APPENDINTARRAY test_appendintarray_deleteintarray 1 2 3" "OK" "Initialize bitmap with values"
  rcall_assert "R64.GETINTARRAY test_appendintarray_deleteintarray" "$(echo -e "1\n2\n3")" "Get array after initialization"
  rcall_assert "R64.DELETEINTARRAY test_appendintarray_deleteintarray 1 3" "OK" "Delete values 1 and 3"
  rcall_assert "R64.GETINTARRAY test_appendintarray_deleteintarray" "2" "Get array after deletion"
}

function test_min_max() {
  print_test_header "test_min_max (64)"

  rcall_assert "R64.MIN test_min_max" "-1" "Get min from empty bitmap"
  rcall_assert "R64.MAX test_min_max" "-1" "Get max from empty bitmap"

  rcall_assert "R64.SETBIT test_min_max 100 1" "0" "Set bit 100"
  rcall_assert "R64.MIN test_min_max" "100" "Get min after setting bit 100"
  rcall_assert "R64.MAX test_min_max" "100" "Get max after setting bit 100"

  rcall_assert "R64.SETBIT test_min_max 0 1" "0" "Set bit 0"
  rcall_assert "R64.MIN test_min_max" "0" "Get min after setting bit 0"
  rcall_assert "R64.MAX test_min_max" "100" "Get max after setting both bits"

  rcall_assert "R64.SETBIT test_min_max 0 0" "1" "Unset bit 0"
  rcall_assert "R64.SETBIT test_min_max 100 0" "1" "Unset bit 100"
  rcall_assert "R64.MIN test_min_max" "-1" "Get min after unsetting all bits"
  rcall_assert "R64.MAX test_min_max" "-1" "Get max after unsetting all bits"
}

function test_bitop_diff() {
  print_test_header "test_bitop_diff (64)"

  # Test 1: Empty/non-existent keys
  rcall_assert "R64.BITOP DIFF diff_res empty_key_1 empty_key_2" "0"

  # Test 2: Basic DIFF operation - X - Y (single Y key)
  rcall_assert "R64.SETINTARRAY bitop_x_2 1 2 3 4 5" "OK" "Set bitmap X with values 1-5"
  rcall_assert "R64.SETINTARRAY bitop_y_1 3 4 5 6 7" "OK" "Set bitmap Y with values 3-7"
  rcall_assert "R64.BITOP DIFF diff_res_2 bitop_x_2 bitop_y_1" "2" "DIFF X - Y"
  rcall_assert "R64.GETINTARRAY diff_res_2" "$(echo -e "1\n2")" "Result should be {1, 2}"

  # Test 3: DIFF with multiple Y keys - X - Y1 - Y2
  rcall_assert "R64.SETINTARRAY bitop_x_3 1 2 3 4 5 6 7 8" "OK" "Set bitmap X with values 1-8"
  rcall_assert "R64.SETINTARRAY bitop_y1_1 2 3" "OK" "Set bitmap Y1 with values 2-3"
  rcall_assert "R64.SETINTARRAY bitop_y2_1 5 6" "OK" "Set bitmap Y2 with values 5-6"
  rcall_assert "R64.BITOP DIFF diff_res_3 bitop_x_3 bitop_y1_1 bitop_y2_1" "4" "DIFF X - Y1 - Y2"
  rcall_assert "R64.GETINTARRAY diff_res_3" "$(echo -e "1\n4\n7\n8")" "Result should be {1, 4, 7, 8}"

  # Test 4: DIFF with three Y keys
  rcall_assert "R64.SETINTARRAY bitop_x_4 1 2 3 4 5 6 7 8 9 10" "OK" "Set bitmap X with values 1-10"
  rcall_assert "R64.SETINTARRAY bitop_y1_2 1 2" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_y2_2 3 4" "OK" "Set bitmap Y2"
  rcall_assert "R64.SETINTARRAY bitop_y3_1 5 6" "OK" "Set bitmap Y3"
  rcall_assert "R64.BITOP DIFF diff_res_4 bitop_x_4 bitop_y1_2 bitop_y2_2 bitop_y3_1" "4" "DIFF X - Y1 - Y2 - Y3"
  rcall_assert "R64.GETINTARRAY diff_res_4" "$(echo -e "7\n8\n9\n10")" "Result should be {7, 8, 9, 10}"

  # Test 5: DIFF where X is completely contained in Y (empty result)
  rcall_assert "R64.SETINTARRAY bitop_x_5 1 2 3" "OK" "Set bitmap X with values 1-3"
  rcall_assert "R64.SETINTARRAY bitop_y_2 1 2 3 4 5" "OK" "Set bitmap Y with superset"
  rcall_assert "R64.BITOP DIFF diff_res_5 bitop_x_5 bitop_y_2" "0" "DIFF where X subset of Y"
  rcall_assert "R64.GETINTARRAY diff_res_5" "" "Result should be empty"

  # Test 6: DIFF where X and Y are disjoint (result equals X)
  rcall_assert "R64.SETINTARRAY bitop_x_6 1 2 3" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_3 7 8 9" "OK" "Set bitmap Y (disjoint)"
  rcall_assert "R64.BITOP DIFF diff_res_6 bitop_x_6 bitop_y_3" "3" "DIFF with disjoint sets"
  rcall_assert "R64.GETINTARRAY diff_res_6" "$(echo -e "1\n2\n3")" "Result should equal X"

  # Test 7: DIFF with empty X bitmap
  rcall_assert "R64.SETINTARRAY bitop_y_4 1 2 3" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF diff_res_7 bitop_x_7 bitop_y_4" "0" "DIFF with empty X"
  rcall_assert "R64.GETINTARRAY diff_res_7" "" "Result should be empty"

  # Test 8: DIFF with empty Y bitmap (result equals X)
  rcall_assert "R64.SETINTARRAY bitop_x_8 1 2 3 4" "OK" "Set bitmap X"
  rcall_assert "R64.BITOP DIFF diff_res_8 bitop_x_8 bitop_y_5" "4" "DIFF with empty Y"
  rcall_assert "R64.GETINTARRAY diff_res_8" "$(echo -e "1\n2\n3\n4")" "Result should equal X"

  # Test 9: DIFF with overlapping Y keys
  rcall_assert "R64.SETINTARRAY bitop_x_9 1 2 3 4 5 6" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y1_3 2 3 4" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_y2_3 3 4 5" "OK" "Set bitmap Y2 (overlaps Y1)"
  rcall_assert "R64.BITOP DIFF diff_res_9 bitop_x_9 bitop_y1_3 bitop_y2_3" "2" "DIFF with overlapping Y keys"
  rcall_assert "R64.GETINTARRAY diff_res_9" "$(echo -e "1\n6")" "Result should be {1, 6}"

  # Test 10: Overwrite existing destination key
  rcall_assert "R64.SETINTARRAY diff_res_10 99 100" "OK" "Pre-populate destination key"
  rcall_assert "R64.SETINTARRAY bitop_x_10 5 6 7" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_6 6" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF diff_res_10 bitop_x_10 bitop_y_6" "2" "DIFF overwrites existing key"
  rcall_assert "R64.GETINTARRAY diff_res_10" "$(echo -e "5\n7")" "Destination should be overwritten, not {99, 100}"

  # Test 11: DIFF with large values
  rcall_assert "R64.SETINTARRAY bitop_x_11 1000 2000 3000 4000" "OK" "Set bitmap X with large values"
  rcall_assert "R64.SETINTARRAY bitop_y_7 2000 3000" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF diff_res_11 bitop_x_11 bitop_y_7" "2" "DIFF with large values"
  rcall_assert "R64.GETINTARRAY diff_res_11" "$(echo -e "1000\n4000")" "Result should be {1000, 4000}"

  # Test 12: DIFF result as input to another operation
  rcall_assert "R64.SETINTARRAY bitop_x_12 1 2 3 4 5" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_8 3 4" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF diff_temp bitop_x_12 bitop_y_8" "3" "First DIFF operation"
  rcall_assert "R64.SETINTARRAY bitop_y_9 1" "OK" "Set another bitmap"
  rcall_assert "R64.BITOP DIFF diff_res_12 diff_temp bitop_y_9" "2" "Chain DIFF operations"
  rcall_assert "R64.GETINTARRAY diff_res_12" "$(echo -e "2\n5")" "Chained result should be {2, 5}"

  # Test 13: Verify X and Y keys are not modified
  rcall_assert "R64.SETINTARRAY bitop_x_13 10 20 30" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_10 20" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF diff_res_13 bitop_x_13 bitop_y_10" "2" "Perform DIFF"
  rcall_assert "R64.GETINTARRAY bitop_x_13" "$(echo -e "10\n20\n30")" "X should remain unchanged"
  rcall_assert "R64.GETINTARRAY bitop_y_10" "20" "Y should remain unchanged"

  # Test 15: DIFF with destination key as middle source key
  rcall_assert "R64.SETINTARRAY bitop_x_14 1 2 3 4 5 6 7 8" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_dest_src_2 2 3 4" "OK" "Set bitmap that will be dest and middle src"
  rcall_assert "R64.SETINTARRAY bitop_y_12 6 7" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF bitop_dest_src_2 bitop_x_14 bitop_dest_src_2 bitop_y_12" "3" "DIFF: X - dest - Y"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_2" "$(echo -e "1\n5\n8")" "Result should be {1, 5, 8}"

  # Test 16: DIFF with destination key as last source key
  rcall_assert "R64.SETINTARRAY bitop_x_15 10 20 30 40 50" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_13 20 30" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_dest_src_3 40" "OK" "Set bitmap that will be dest and last src"
  rcall_assert "R64.BITOP DIFF bitop_dest_src_3 bitop_x_15 bitop_y_13 bitop_dest_src_3" "2" "DIFF: X - Y - dest"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_3" "$(echo -e "10\n50")" "Result should be {10, 50}"

  # Test 17: DIFF with destination key appearing multiple times in sources
  rcall_assert "R64.SETINTARRAY bitop_dest_src_4 5 10 15 20" "OK" "Set bitmap for dest/multi-src test"
  rcall_assert "R64.SETINTARRAY bitop_x_16 1 2 3 4 5 10 15 20 25 30" "OK" "Set bitmap X"
  rcall_assert "R64.BITOP DIFF bitop_dest_src_4 bitop_x_16 bitop_dest_src_4 bitop_dest_src_4" "6" "DIFF: X - dest - dest"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_4" "$(echo -e "1\n2\n3\n4\n25\n30")" "Result should be X minus original dest values"

  # Test 18: DIFF with dest as first source and empty result
  rcall_assert "R64.SETINTARRAY bitop_dest_src_5 7 8 9" "OK" "Set bitmap for dest/src"
  rcall_assert "R64.SETINTARRAY bitop_y_14 7 8 9 10 11" "OK" "Set bitmap Y (superset)"
  rcall_assert "R64.BITOP DIFF bitop_dest_src_5 bitop_dest_src_5 bitop_y_14" "0" "DIFF with dest as src, empty result"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_5" "" "Dest should be empty after operation"
}

function test_bitop_diff1() {
  print_test_header "test_bitop_diff1 (64)"

  # Test 1: Empty/non-existent keys - should return 0
  rcall_assert "R64.BITOP DIFF1 diff1_res_1 empty_key_1 empty_key_2" "0" "DIFF1 with empty keys"

  # Test 2: Basic DIFF1 operation - (Y OR Z) - X (single Y key)
  rcall_assert "R64.SETINTARRAY bitop_x_d1_1 3 4 5" "OK" "Set bitmap X with values 3-5"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_1 1 2 3 4 5 6 7" "OK" "Set bitmap Y with values 1-7"
  rcall_assert "R64.BITOP DIFF1 diff1_res_2 bitop_x_d1_1 bitop_y_d1_1" "4" "DIFF1 Y - X"
  rcall_assert "R64.GETINTARRAY diff1_res_2" "$(echo -e "1\n2\n6\n7")" "Result should be {1, 2, 6, 7}"

  # Test 3: DIFF1 with multiple Y keys - (Y1 OR Y2) - X
  rcall_assert "R64.SETINTARRAY bitop_x_d1_2 2 3 5 6" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y1_d1_1 1 2 3 4" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_y2_d1_1 5 6 7 8" "OK" "Set bitmap Y2"
  rcall_assert "R64.BITOP DIFF1 diff1_res_3 bitop_x_d1_2 bitop_y1_d1_1 bitop_y2_d1_1" "4" "DIFF1 (Y1 OR Y2) - X"
  rcall_assert "R64.GETINTARRAY diff1_res_3" "$(echo -e "1\n4\n7\n8")" "Result should be {1, 4, 7, 8}"

  # Test 4: DIFF1 with three Y keys - (Y1 OR Y2 OR Y3) - X
  rcall_assert "R64.SETINTARRAY bitop_x_d1_3 1 2 5 6 9 10" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y1_d1_2 1 2 3" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_y2_d1_2 4 5 6" "OK" "Set bitmap Y2"
  rcall_assert "R64.SETINTARRAY bitop_y3_d1_1 7 8 9" "OK" "Set bitmap Y3"
  rcall_assert "R64.BITOP DIFF1 diff1_res_4 bitop_x_d1_3 bitop_y1_d1_2 bitop_y2_d1_2 bitop_y3_d1_1" "4" "DIFF1 (Y1 OR Y2 OR Y3) - X"
  rcall_assert "R64.GETINTARRAY diff1_res_4" "$(echo -e "3\n4\n7\n8")" "Result should be {3, 4, 7, 8}"

  # Test 5: DIFF1 where Y is completely contained in X (empty result)
  rcall_assert "R64.SETINTARRAY bitop_x_d1_4 1 2 3 4 5" "OK" "Set bitmap X with superset"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_2 2 3 4" "OK" "Set bitmap Y (subset)"
  rcall_assert "R64.BITOP DIFF1 diff1_res_5 bitop_x_d1_4 bitop_y_d1_2" "0" "DIFF1 where Y subset of X"
  rcall_assert "R64.GETINTARRAY diff1_res_5" "" "Result should be empty"

  # Test 6: DIFF1 where X and Y are disjoint (result equals Y)
  rcall_assert "R64.SETINTARRAY bitop_x_d1_5 1 2 3" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_3 7 8 9" "OK" "Set bitmap Y (disjoint)"
  rcall_assert "R64.BITOP DIFF1 diff1_res_6 bitop_x_d1_5 bitop_y_d1_3" "3" "DIFF1 with disjoint sets"
  rcall_assert "R64.GETINTARRAY diff1_res_6" "$(echo -e "7\n8\n9")" "Result should equal Y"

  # Test 7: DIFF1 with empty X bitmap (result equals Y)
  rcall_assert "R64.SETINTARRAY bitop_y_d1_4 5 6 7 8" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF1 diff1_res_7 bitop_x_d1_6 bitop_y_d1_4" "4" "DIFF1 with empty X"
  rcall_assert "R64.GETINTARRAY diff1_res_7" "$(echo -e "5\n6\n7\n8")" "Result should equal Y"

  # Test 8: DIFF1 with empty Y bitmap (empty result)
  rcall_assert "R64.SETINTARRAY bitop_x_d1_7 1 2 3 4" "OK" "Set bitmap X"
  rcall_assert "R64.BITOP DIFF1 diff1_res_8 bitop_x_d1_7 bitop_y_d1_5" "0" "DIFF1 with empty Y"
  rcall_assert "R64.GETINTARRAY diff1_res_8" "" "Result should be empty"

  # Test 9: DIFF1 with empty Y bitmaps (empty result)
  rcall_assert "R64.SETINTARRAY bitop_x_d1_8 10 20 30" "OK" "Set bitmap X"
  rcall_assert "R64.BITOP DIFF1 diff1_res_9 bitop_x_d1_8 bitop_y_d1_6 bitop_y_d1_7" "0" "DIFF1 with all Y empty"
  rcall_assert "R64.GETINTARRAY diff1_res_9" "" "Result should be empty"

  # Test 10: DIFF1 with overlapping Y keys
  rcall_assert "R64.SETINTARRAY bitop_x_d1_9 3 4 5" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y1_d1_3 1 2 3 4" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_y2_d1_3 4 5 6 7" "OK" "Set bitmap Y2 (overlaps Y1)"
  rcall_assert "R64.BITOP DIFF1 diff1_res_10 bitop_x_d1_9 bitop_y1_d1_3 bitop_y2_d1_3" "4" "DIFF1 with overlapping Y keys"
  rcall_assert "R64.GETINTARRAY diff1_res_10" "$(echo -e "1\n2\n6\n7")" "Result should be {1, 2, 6, 7}"

  # Test 11: Overwrite existing destination key
  rcall_assert "R64.SETINTARRAY diff1_res_11 99 100" "OK" "Pre-populate destination key"
  rcall_assert "R64.SETINTARRAY bitop_x_d1_10 5 6" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_8 5 6 7 8" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF1 diff1_res_11 bitop_x_d1_10 bitop_y_d1_8" "2" "DIFF1 overwrites existing key"
  rcall_assert "R64.GETINTARRAY diff1_res_11" "$(echo -e "7\n8")" "Destination should be overwritten, not {99, 100}"

  # Test 12: DIFF1 with large values
  rcall_assert "R64.SETINTARRAY bitop_x_d1_11 2000 3000" "OK" "Set bitmap X with large values"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_9 1000 2000 3000 4000" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF1 diff1_res_12 bitop_x_d1_11 bitop_y_d1_9" "2" "DIFF1 with large values"
  rcall_assert "R64.GETINTARRAY diff1_res_12" "$(echo -e "1000\n4000")" "Result should be {1000, 4000}"

  # Test 13: DIFF1 result as input to another operation
  rcall_assert "R64.SETINTARRAY bitop_x_d1_12 3 4" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_10 1 2 3 4 5" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF1 diff1_temp bitop_x_d1_12 bitop_y_d1_10" "3" "First DIFF1 operation"
  rcall_assert "R64.SETINTARRAY bitop_x_d1_13 1" "OK" "Set another bitmap"
  rcall_assert "R64.BITOP DIFF1 diff1_res_13 bitop_x_d1_13 diff1_temp" "2" "Chain DIFF1 operations"
  rcall_assert "R64.GETINTARRAY diff1_res_13" "$(echo -e "2\n5")" "Chained result should be {2, 5}"

  # Test 14: Verify X and Y keys are not modified
  rcall_assert "R64.SETINTARRAY bitop_x_d1_14 20" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_11 10 20 30" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF1 diff1_res_14 bitop_x_d1_14 bitop_y_d1_11" "2" "Perform DIFF1"
  rcall_assert "R64.GETINTARRAY bitop_x_d1_14" "20" "X should remain unchanged"
  rcall_assert "R64.GETINTARRAY bitop_y_d1_11" "$(echo -e "10\n20\n30")" "Y should remain unchanged"

  # Test 15: DIFF1 with destination key as X source (in-place operation)
  rcall_assert "R64.SETINTARRAY bitop_dest_src_d1_1 3 4 5" "OK" "Set bitmap that will be both dest and X"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_12 1 2 3 4 5 6" "OK" "Set bitmap Y"
  rcall_assert "R64.BITOP DIFF1 bitop_dest_src_d1_1 bitop_dest_src_d1_1 bitop_y_d1_12" "3" "DIFF1 with dest as X (in-place)"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_d1_1" "$(echo -e "1\n2\n6")" "Dest should be modified in-place to {1, 2, 6}"

  # Test 16: DIFF1 with destination key as first Y source
  rcall_assert "R64.SETINTARRAY bitop_x_d1_15 2 3 4" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_dest_src_d1_2 1 2 3 4 5 6" "OK" "Set bitmap that will be dest and first Y"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_13 6 7 8" "OK" "Set bitmap Y2"
  rcall_assert "R64.BITOP DIFF1 bitop_dest_src_d1_2 bitop_x_d1_15 bitop_dest_src_d1_2 bitop_y_d1_13" "5" "DIFF1: (dest OR Y2) - X"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_d1_2" "$(echo -e "1\n5\n6\n7\n8")" "Result should be {1, 5, 6, 7, 8}"

  # Test 17: DIFF1 with destination key as middle Y source
  rcall_assert "R64.SETINTARRAY bitop_x_d1_16 5 10 15" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_14 1 5 10" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_dest_src_d1_3 10 15 20" "OK" "Set bitmap that will be dest and middle Y"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_15 15 20 25" "OK" "Set bitmap Y3"
  rcall_assert "R64.BITOP DIFF1 bitop_dest_src_d1_3 bitop_x_d1_16 bitop_y_d1_14 bitop_dest_src_d1_3 bitop_y_d1_15" "3" "DIFF1: (Y1 OR dest OR Y3) - X"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_d1_3" "$(echo -e "1\n20\n25")" "Result should be {1, 20, 25}"

  # Test 18: DIFF1 with destination key as last Y source
  rcall_assert "R64.SETINTARRAY bitop_x_d1_17 20 30" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_16 10 20 30" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_dest_src_d1_4 30 40 50" "OK" "Set bitmap that will be dest and last Y"
  rcall_assert "R64.BITOP DIFF1 bitop_dest_src_d1_4 bitop_x_d1_17 bitop_y_d1_16 bitop_dest_src_d1_4" "3" "DIFF1: (Y1 OR dest) - X"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_d1_4" "$(echo -e "10\n40\n50")" "Result should be {10, 40, 50}"

  # Test 19: DIFF1 with destination key appearing multiple times in Y sources
  rcall_assert "R64.SETINTARRAY bitop_x_d1_18 10 20" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_dest_src_d1_5 5 10 15 20 25" "OK" "Set bitmap for dest/multi-Y test"
  rcall_assert "R64.BITOP DIFF1 bitop_dest_src_d1_5 bitop_x_d1_18 bitop_dest_src_d1_5 bitop_dest_src_d1_5" "3" "DIFF1: (dest OR dest) - X"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_d1_5" "$(echo -e "5\n15\n25")" "Result should be {5, 15, 25}"

  # Test 20: DIFF1 with dest as Y and empty result
  rcall_assert "R64.SETINTARRAY bitop_dest_src_d1_6 7 8 9" "OK" "Set bitmap for dest/Y"
  rcall_assert "R64.SETINTARRAY bitop_x_d1_19 7 8 9 10 11" "OK" "Set bitmap X (superset)"
  rcall_assert "R64.BITOP DIFF1 bitop_dest_src_d1_6 bitop_x_d1_19 bitop_dest_src_d1_6" "0" "DIFF1 with dest as Y, empty result"
  rcall_assert "R64.GETINTARRAY bitop_dest_src_d1_6" "" "Dest should be empty after operation"

  # Test 21: DIFF1 with single Y where Y equals X (empty result)
  rcall_assert "R64.SETINTARRAY bitop_x_d1_20 100 200 300" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y_d1_17 100 200 300" "OK" "Set bitmap Y identical to X"
  rcall_assert "R64.BITOP DIFF1 diff1_res_21 bitop_x_d1_20 bitop_y_d1_17" "0" "DIFF1 where Y equals X"
  rcall_assert "R64.GETINTARRAY diff1_res_21" "" "Result should be empty"

  # Test 22: DIFF1 with multiple Y keys where union equals X
  rcall_assert "R64.SETINTARRAY bitop_x_d1_21 1 2 3 4 5 6" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y1_d1_4 1 2 3" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_y2_d1_4 4 5 6" "OK" "Set bitmap Y2"
  rcall_assert "R64.BITOP DIFF1 diff1_res_22 bitop_x_d1_21 bitop_y1_d1_4 bitop_y2_d1_4" "0" "DIFF1 where Y1 OR Y2 equals X"
  rcall_assert "R64.GETINTARRAY diff1_res_22" "" "Result should be empty"

  # Test 23: DIFF1 with four Y keys
  rcall_assert "R64.SETINTARRAY bitop_x_d1_22 5 10 15 20 25 30" "OK" "Set bitmap X"
  rcall_assert "R64.SETINTARRAY bitop_y1_d1_5 1 5" "OK" "Set bitmap Y1"
  rcall_assert "R64.SETINTARRAY bitop_y2_d1_5 10 11" "OK" "Set bitmap Y2"
  rcall_assert "R64.SETINTARRAY bitop_y3_d1_2 15 16" "OK" "Set bitmap Y3"
  rcall_assert "R64.SETINTARRAY bitop_y4_d1_1 20 21" "OK" "Set bitmap Y4"
  rcall_assert "R64.BITOP DIFF1 diff1_res_23 bitop_x_d1_22 bitop_y1_d1_5 bitop_y2_d1_5 bitop_y3_d1_2 bitop_y4_d1_1" "4" "DIFF1 with four Y keys"
  rcall_assert "R64.GETINTARRAY diff1_res_23" "$(echo -e "1\n11\n16\n21")" "Result should be {1, 11, 16, 21}"
}

function test_bitop_one() {
  print_test_header "test_bitop_one (64)"

  # Test 1: Empty bitmap array
  rcall_assert "R64.BITOP ONE test_bitop_one64_result_empty" "ERR wrong number of arguments for 'R64.BITOP' command" "BITOP ONE with no source bitmaps"

  # Test 2: Single bitmap
  rcall_assert "R64.BITOP ONE test_bitop_one64_result_single test_bitop_one64_key1" "ERR wrong number of arguments for 'R64.BITOP' command" "BITOP ONE with single bitmap"

  # Test 3: Two non-overlapping bitmaps
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key1 1 3 5" "OK" "Set bits in test_bitop_one64_key1"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key2 2 4 6" "OK" "Set bits in test_bitop_one64_key2"

  rcall_assert "R64.BITOP ONE test_bitop_one64_result_non_overlap test_bitop_one64_key1 test_bitop_one64_key2" "6" "BITOP ONE with non-overlapping bitmaps"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_result_non_overlap" "1\n2\n3\n4\n5\n6" "Result should contain all bits from both keys"

  # Test 4: Two overlapping bitmaps
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key3 1 2 3" "OK" "Set bits in test_bitop_one64_key3"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key4 3 4 5" "OK" "Set bits in test_bitop_one64_key4"

  rcall_assert "R64.BITOP ONE test_bitop_one64_result_overlap test_bitop_one64_key3 test_bitop_one64_key4" "4" "BITOP ONE with overlapping bitmaps"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_result_overlap" "1\n2\n4\n5" "Result should contain only non-overlapping bits"

  # Test 5: Three bitmaps - Redis example
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_a 0 4 5 6" "OK" "Set bits in test_bitop_one64_key_a"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_b 1 5 6" "OK" "Set bits in test_bitop_one64_key_b"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_c 2 3 5 6 7" "OK" "Set bits in test_bitop_one64_key_c"

  rcall_assert "R64.BITOP ONE test_bitop_one64_result_three test_bitop_one64_key_a test_bitop_one64_key_b test_bitop_one64_key_c" "6" "BITOP ONE with three bitmaps"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_result_three" "0\n1\n2\n3\n4\n7" "Result should contain bits appearing exactly once"

  # Test 6: All bitmaps have same bits
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_same1 10 20 30" "OK" "Set bits in test_bitop_one64_key_same1"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_same2 10 20 30" "OK" "Set bits in test_bitop_one64_key_same2"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_same3 10 20 30" "OK" "Set bits in test_bitop_one64_key_same3"

  rcall_assert "R64.BITOP ONE test_bitop_one64_result_same test_bitop_one64_key_same1 test_bitop_one64_key_same2 test_bitop_one64_key_same3" "0" "BITOP ONE with identical bitmaps"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_result_same" "" "Result should be empty array"

  # Test 7: One empty bitmap among others
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_with_bits1 1 2 3" "OK" "Set bits in test_bitop_one64_key_with_bits1"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_with_bits2 3 4 5" "OK" "Set bits in test_bitop_one64_key_with_bits2"

  rcall "DEL test_bitop_one64_key_empty"
  rcall_assert "R64.BITOP ONE test_bitop_one64_result_mixed test_bitop_one64_key_with_bits1 test_bitop_one64_key_empty test_bitop_one64_key_with_bits2" "4" "BITOP ONE with one empty bitmap"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_result_mixed" "1\n2\n4\n5" "Result should contain non-overlapping bits"

  # Test 8: Complex overlaps with four bitmaps
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_complex1 1 2 3 4 5" "OK" "Set bits in test_bitop_one64_key_complex1"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_complex2 2 3 4 6 7" "OK" "Set bits in test_bitop_one64_key_complex2"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_complex3 3 4 5 7 8" "OK" "Set bits in test_bitop_one64_key_complex3"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_complex4 4 5 6 8 9" "OK" "Set bits in test_bitop_one64_key_complex4"

  rcall_assert "R64.BITOP ONE test_bitop_one64_result_complex test_bitop_one64_key_complex1 test_bitop_one64_key_complex2 test_bitop_one64_key_complex3 test_bitop_one64_key_complex4" "2" "BITOP ONE with complex overlaps"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_result_complex" "1\n9" "Result should contain only bits appearing exactly once"

  # Test 9: Large bit values
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_large1 1000000 2000000" "OK" "Set large bits in key_large1"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_large2 2000000 3000000" "OK" "Set large bits in key_large2"

  rcall_assert "R64.BITOP ONE test_bitop_one64_result_large test_bitop_one64_key_large1 test_bitop_one64_key_large2" "2" "BITOP ONE with large bit values"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_result_large" "1000000\n3000000" "Result should contain non-overlapping large bits"

  # Test 10: Destination already has content
  rcall_assert "R64.SETINTARRAY test_bitop_one64_result_preexist 100 200 300" "OK" "Set bits in test_bitop_one64_result_preexist"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_overwrite1 1 2" "OK" "Set bits in test_bitop_one64_key_overwrite1"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_overwrite2 2 3" "OK" "Set bits in test_bitop_one64_key_overwrite2"

  rcall_assert "R64.BITOP ONE test_bitop_one64_result_preexist test_bitop_one64_key_overwrite1 test_bitop_one64_key_overwrite2" "2" "BITOP ONE should overwrite destination"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_result_preexist" "1\n3" "Result should only contain new operation result"

  # Test 11: Input bitmaps should remain unchanged
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_immutable1 10 20 30" "OK" "Set bits in test_bitop_one64_key_immutable1"
  rcall_assert "R64.SETINTARRAY test_bitop_one64_key_immutable2 20 30 40" "OK" "Set bits in test_bitop_one64_key_immutable2"

  rcall_assert "R64.BITOP ONE test_bitop_one64_result_immutable test_bitop_one64_key_immutable1 test_bitop_one64_key_immutable2" "2" "BITOP ONE operation"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_key_immutable1" "10\n20\n30" "key_immutable1 should remain unchanged"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_key_immutable2" "20\n30\n40" "key_immutable2 should remain unchanged"
  rcall_assert "R64.GETINTARRAY test_bitop_one64_result_immutable" "10\n40" "Result should contain bits appearing exactly once"
}

function test_diff() {
  print_test_header "test_diff (64)"

  rcall_assert "R64.DIFF test_diff_res" "ERR wrong number of arguments for 'R64.DIFF' command" "DIFF with wrong number of arguments"
  rcall_assert "R64.DIFF test_diff_res empty_key_1 empty_key_2" "${ERRORMSG_KEY_MISSED}" "DIFF with empty keys"

  rcall_assert "R64.SETINTARRAY test_diff_1 1 2 3 4" "OK" "Set first array for diff test"
  rcall_assert "R64.SETINTARRAY test_diff_2 3 4 5 6" "OK" "Set second array for diff test"

  rcall_assert "R64.DIFF test_diff_res test_diff_1 test_diff_2" "OK" "Compute difference test_diff_1 - test_diff_2"
  rcall_assert "R64.GETINTARRAY test_diff_res" "$(echo -e "1\n2")" "Get result of first difference"

  rcall_assert "R64.DIFF test_diff_res test_diff_2 test_diff_1" "OK" "Compute difference test_diff_2 - test_diff_1"
  rcall_assert "R64.GETINTARRAY test_diff_res" "$(echo -e "5\n6")" "Get result of second difference"
}

function test_setrage() {
  print_test_header "test_setrage (64)"

  rcall_assert "R64.SETRANGE test_setrange 0 5" "OK" "Should set range from 0 to 5"
  rcall_assert "R64.GETINTARRAY test_setrange" "$(echo -e "0\n1\n2\n3\n4")" "Validate range from 0 5"
  rcall_assert "R64.SETRANGE test_setrange 5 5" "OK" "Should handle zero range"
}

function test_clear() {
  print_test_header "test_clear"

  rcall_assert "R64.SETINTARRAY test_clear 1 2 3 4 5" "OK" "Set bit for deletion test"
  rcall_assert "R64.CLEAR test_clear" "5" "Clear key test_clear"
  rcall_assert "R64.BITCOUNT test_clear" "0" "Should be empty after clear"
  rcall_assert "R64.CLEAR test_clear_not_exists" "" "Should return nil for non exist key"
}

function test_optimize_nokey() {
  print_test_header "test_optimize nokey (64)"
  rcall_assert "R64.OPTIMIZE no-key" "${ERRORMSG_KEY_MISSED}" "Optimize non-existent key"
}

function test_setfull() {
  print_test_header "test_setfull (64)"

  rcall "del foo"
  rcall "R64.SETFULL foo"
  rcall_assert "R64.BITCOUNT foo" "18446744073709551615" "Count bits in full bitmap"
}

function test_del() {
  print_test_header "test_del (64)"

  rcall_assert "R64.SETBIT test_del 0 1" "0" "Set bit for deletion test"
  rcall_assert "DEL test_del" "1" "Delete key test_del"
}

function test_contains() {
  print_test_header "test_contains (64)"

  # Setup test data
  rcall_assert "R64.SETINTARRAY test_contains1 1 2 3 4 5" "OK" "Setup test_contains1 with [1,2,3,4,5]"
  rcall_assert "R64.SETINTARRAY test_contains2 2 3" "OK" "Setup test_contains2 with [2,3]"
  rcall_assert "R64.SETINTARRAY test_contains3 3 4 6" "OK" "Setup test_contains3 with [3,4,6]"
  rcall_assert "R64.SETINTARRAY test_contains4 1 2 3 4 5" "OK" "Setup test_contains4 with [1,2,3,4,5] (same as test_contains1)"
  rcall_assert "R64.SETBIT test_contains5 0 1" "0" "Setup empty test_contains5"
  rcall_assert "R64.CLEAR test_contains5" "1" "Cleanup test_contains5"
  rcall_assert "R64.SETINTARRAY test_contains6 7 8 9" "OK" "Setup test_contains6 with [7,8,9] (no intersection with test_contains1)"
  rcall_assert "R64.SETINTARRAY test_contains_eq1 1 2 3 4 5" "OK" "Setup eq1 with [1,2,3,4,5]"
  rcall_assert "R64.SETINTARRAY test_contains_eq2 1 2 3 4 5" "OK" "Setup eq2 with [1,2,3,4,5] (same as eq1)"

  # Test basic intersection (default mode - should be NONE)
  rcall_assert "R64.CONTAINS test_contains1 test_contains2" "1" "test_contains1 contains some elements from test_contains2 (intersection exists)"
  rcall_assert "R64.CONTAINS test_contains1 test_contains6" "0" "test_contains1 doesn't intersect with test_contains6"
  rcall_assert "R64.CONTAINS test_contains5 test_contains1" "0" "Empty test_contains5 doesn't intersect with test_contains1"
  rcall_assert "R64.CONTAINS test_contains1 test_contains5" "0" "test_contains1 doesn't intersect with empty test_contains5"

  # Test ALL mode (test_contains2 is subset of test_contains1)
  rcall_assert "R64.CONTAINS test_contains1 test_contains2 ALL" "1" "test_contains2 [2,3] is subset of test_contains1 [1,2,3,4,5]"
  rcall_assert "R64.CONTAINS test_contains1 test_contains3 ALL" "0" "test_contains3 [3,4,6] is not subset of test_contains1 (6 not in test_contains1)"
  rcall_assert "R64.CONTAINS test_contains1 test_contains4 ALL" "1" "test_contains4 [1,2,3,4,5] is subset of test_contains1 [1,2,3,4,5] (equal sets)"
  rcall_assert "R64.CONTAINS test_contains1 test_contains5 ALL" "1" "Empty test_contains5 is subset of any bitmap"
  rcall_assert "R64.CONTAINS test_contains5 test_contains1 ALL" "0" "Non-empty test_contains1 is not subset of empty test_contains5"

  # Test ALL_STRICT mode (test_contains2 is strict subset of test_contains1)
  rcall_assert "R64.CONTAINS test_contains1 test_contains2 ALL_STRICT" "1" "test_contains2 [2,3] is strict subset of test_contains1 [1,2,3,4,5]"
  rcall_assert "R64.CONTAINS test_contains1 test_contains3 ALL_STRICT" "0" "test_contains3 [3,4,6] is not subset of test_contains1"
  rcall_assert "R64.CONTAINS test_contains1 test_contains4 ALL_STRICT" "0" "test_contains4 [1,2,3,4,5] is not strict subset of test_contains1 (equal sets)"
  rcall_assert "R64.CONTAINS test_contains1 test_contains5 ALL_STRICT" "1" "Empty test_contains5 is strict subset of non-empty test_contains1"
  rcall_assert "R64.CONTAINS test_contains5 test_contains1 ALL_STRICT" "0" "Non-empty test_contains1 is not strict subset of empty test_contains5"
  rcall_assert "R64.CONTAINS test_contains5 test_contains5 ALL_STRICT" "0" "Empty bitmap is not strict subset of itself"

  # Test EQ mode identical bitmaps
  rcall_assert "R64.CONTAINS test_contains_eq1 test_contains_eq2 EQ" "1" "test_contains_eq1 and test_contains_eq2 are equal"
  rcall_assert "R64.CONTAINS test_contains_eq2 test_contains_eq1 EQ" "1" "test_contains_eq2 and test_contains_eq1 are equal (symmetry test)"

  # Test EQ mode non-equal bitmaps
  rcall_assert "R64.CONTAINS test_contains_eq1 test_contains6 EQ" "0" "test_contains_eq1 [1,2,3,4,5] is not equal to test_contains6"
  rcall_assert "R64.CONTAINS test_contains_eq1 test_contains5 EQ" "0" "test_contains_eq1 [1,2,3,4,5] is not equal to test_contains5"

  # Test with single element bitmaps
  rcall_assert "R64.SETINTARRAY test_contains_single1 3" "OK" "Setup single element bitmap with [3]"
  rcall_assert "R64.SETINTARRAY test_contains_single2 7" "OK" "Setup single element bitmap with [7]"

  rcall_assert "R64.CONTAINS test_contains1 test_contains_single1" "1" "test_contains1 intersects with test_contains_single1 [3]"
  rcall_assert "R64.CONTAINS test_contains1 test_contains_single2" "0" "test_contains1 doesn't intersect with test_contains_single2 [7]"
  rcall_assert "R64.CONTAINS test_contains1 test_contains_single1 ALL" "1" "test_contains_single1 [3] is subset of test_contains1"
  rcall_assert "R64.CONTAINS test_contains1 test_contains_single2 ALL" "0" "test_contains_single2 [7] is not subset of test_contains1"
  rcall_assert "R64.CONTAINS test_contains1 test_contains_single1 ALL_STRICT" "1" "test_contains_single1 [3] is strict subset of test_contains1"

  # Test error cases
  rcall_assert "R64.CONTAINS nonexistent test_contains1" "${ERRORMSG_KEY_MISSED}" "Should return error for non-existent first key"
  rcall_assert "R64.CONTAINS test_contains1 nonexistent" "${ERRORMSG_KEY_MISSED}" "Should return error for non-existent second key"
  rcall_assert "R64.CONTAINS nonexistent1 nonexistent2" "${ERRORMSG_KEY_MISSED}" "Should return error when both keys don't exist"

  # Test invalid mode
  rcall_assert "R64.CONTAINS test_contains1 test_contains2 INVALID_MODE" "ERR invalid mode argument: INVALID_MODE" "Should return error for invalid mode"
  rcall_assert "R64.CONTAINS test_contains1 test_contains2 all" "ERR invalid mode argument: all" "Should return error for lowercase mode (case sensitive)"

  # Test with larger datasets
  rcall_assert "R64.SETINTARRAY test_contains_large1 $(seq 1 1000 | tr '\n' ' ')" "OK" "Setup large test_contains1 with 1-1000"
  rcall_assert "R64.SETINTARRAY test_contains_large2 $(seq 100 200 | tr '\n' ' ')" "OK" "Setup large test_contains2 with 100-200"
  rcall_assert "R64.SETINTARRAY test_contains_large3 $(seq 1001 1100 | tr '\n' ' ')" "OK" "Setup large test_contains3 with 1001-1100"

  rcall_assert "R64.CONTAINS test_contains_large1 test_contains_large2" "1" "Large bitmaps intersection test"
  rcall_assert "R64.CONTAINS test_contains_large1 test_contains_large3" "0" "Large bitmaps no intersection test"
  rcall_assert "R64.CONTAINS test_contains_large1 test_contains_large2 ALL" "1" "Large bitmap subset test"
  rcall_assert "R64.CONTAINS test_contains_large1 test_contains_large3 ALL" "0" "Large bitmap not subset test"
  rcall_assert "R64.CONTAINS test_contains_large1 test_contains_large2 ALL_STRICT" "1" "Large bitmap strict subset test"

  # Test with ranges
  rcall_assert "R64.SETINTARRAY test_contains_range1 1 5 10 15 20" "OK" "Setup sparse range bitmap"
  rcall_assert "R64.SETINTARRAY test_contains_range2 5 15" "OK" "Setup subset range bitmap"
  rcall_assert "R64.SETINTARRAY test_contains_range3 5 25" "OK" "Setup partial overlap range bitmap"

  rcall_assert "R64.CONTAINS test_contains_range1 test_contains_range2" "1" "Sparse range intersection test"
  rcall_assert "R64.CONTAINS test_contains_range1 test_contains_range3" "1" "Partial overlap intersection test"
  rcall_assert "R64.CONTAINS test_contains_range1 test_contains_range2 ALL" "1" "Sparse range subset test"
  rcall_assert "R64.CONTAINS test_contains_range1 test_contains_range3 ALL" "0" "Partial overlap not subset test"
  rcall_assert "R64.CONTAINS test_contains_range1 test_contains_range2 ALL_STRICT" "1" "Sparse range strict subset test"
}

function test_jaccard() {
  print_test_header "test_jaccard (64)"

  # Setup test data
  rcall_assert "R64.SETINTARRAY jaccard1 1 2 3 4 5" "OK" "Setup jaccard1 with [1,2,3,4,5]"
  rcall_assert "R64.SETINTARRAY jaccard2 3 4 5 6 7" "OK" "Setup jaccard2 with [3,4,5,6,7]"
  rcall_assert "R64.SETINTARRAY jaccard3 1 2 3" "OK" "Setup jaccard3 with [1,2,3]"
  rcall_assert "R64.SETBIT jaccard_empty1 0 1" "0" "Setup jaccard_empty1"
  rcall_assert "R64.CLEAR jaccard_empty1" "1" "Cleanup jaccard_empty1"
  rcall_assert "R64.SETBIT jaccard_empty2 0 1" "0" "Setup jaccard_empty2"
  rcall_assert "R64.CLEAR jaccard_empty2" "1" "Cleanup jaccard_empty2"
  rcall_assert "R64.SETINTARRAY jaccard_single 42" "OK" "Setup jaccard_single with [42]"
  rcall_assert "R64.SETINTARRAY jaccard_single2 42" "OK" "Setup jaccard_single2 with [42]"
  rcall_assert "R64.SETINTARRAY jaccard_no_overlap1 1 2" "OK" "Setup jaccard_no_overlap1 with [1,2]"
  rcall_assert "R64.SETINTARRAY jaccard_no_overlap2 3 4" "OK" "Setup jaccard_no_overlap2 with [3,4]"

  # Test cases
  rcall_assert "R64.JACCARD jaccard1 jaccard2" "0.42857142857142855" "Jaccard index of [1,2,3,4,5] and [3,4,5,6,7]"
  rcall_assert "R64.JACCARD jaccard3 jaccard3" "1" "Jaccard index of identical sets"
  rcall_assert "R64.JACCARD jaccard3 jaccard1" "0.6" "Jaccard index where one set is subset of another"
  rcall_assert "R64.JACCARD jaccard_no_overlap1 jaccard_no_overlap2" "0" "Jaccard index with no overlap"
  rcall_assert "R64.JACCARD jaccard_empty1 jaccard_empty2" "-1" "Jaccard index of two empty bitmaps"
  rcall_assert "R64.JACCARD jaccard3 jaccard_empty1" "0" "Jaccard index with one empty bitmap"
  rcall_assert "R64.JACCARD jaccard_single jaccard_single2" "1" "Jaccard index of identical single element bitmaps"
  rcall_assert "R64.JACCARD jaccard_no_overlap1 jaccard_single" "0" "Jaccard index with single element and no overlap"
  rcall_assert "R64.JACCARD nonexistent1 nonexistent2" "${ERRORMSG_KEY_MISSED}" "Jaccard index with non-existent keys"
  rcall_assert "R64.JACCARD jaccard1 nonexistent" "${ERRORMSG_KEY_MISSED}" "Jaccard index with one non-existent key"
}

function test_stat() {
  print_test_header "test_stat (64)"

  rcall_assert "R64.SETBIT test_stat 100 1" "0" "Set bit 100 for stat test"

  EXPECTED_STAT=$'type: bitmap64\ncardinality: 1\nnumber of containers: 1\nmax value: 100\nmin value: 100
number of array containers: 1\n\tarray container values: 1\n\tarray container bytes: 2
bitset  containers: 0\n\tbitset  container values: 0\n\tbitset  container bytes: 0
run containers: 0\n\trun container values: 0\n\trun container bytes: 0'

  rcall_assert "R.STAT test_stat" "$EXPECTED_STAT" "Get bitmap statistics"

  EXPECTED_STAT=$'{"type":"bitmap64","cardinality":"1","number_of_containers":"1","max_value":"100","min_value":"100","array_container":{"number_of_containers":"1","container_cardinality":"1","container_allocated_bytes":"2"},"bitset_container":{"number_of_containers":"0","container_cardinality":"0","container_allocated_bytes":"0"},"run_container":{"number_of_containers":"0","container_cardinality":"0","container_allocated_bytes":"0"}}'

  rcall_assert "R.STAT test_stat JSON" "$EXPECTED_STAT" "Get bitmap statistics (json)"
}

function test_save() {
  print_test_header "test_save (64)"
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
test_setrage
test_clear
test_min_max
test_bitop_diff
test_bitop_diff1
test_bitop_one
test_diff
test_optimize_nokey
# test_setfull
test_del
test_contains
test_jaccard
test_stat
test_save
