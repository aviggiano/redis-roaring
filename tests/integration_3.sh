#!/usr/bin/env bash

set -eu

. "$(dirname "$0")/helper.sh"

function test_setbit_getbit() {
  print_test_header "test_setbit_getbit (64)"

  rcall_assert "R64.SETBIT test_setbit_getbit 0 1" "0" "Set bit at position 0 to 1"
  rcall_assert "R64.GETBIT test_setbit_getbit 0" "1" "Get bit at position 0"

  rcall_assert "R64.SETBIT test_setbit_getbit 0" "ERR wrong number of arguments for 'R64.SETBIT' command" "SETBIT with wrong number of arguments"
  rcall_assert "R64.GETBIT key 0 1" "ERR wrong number of arguments for 'R64.GETBIT' command" "GETBIT with wrong number of arguments"
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

function test_bitop_one() {
  print_test_header "test_bitop_one (64)"

  # Test 1: Empty bitmap array
  rcall_assert "R.BITOP ONE test_bitop_one64_result_empty" "ERR wrong number of arguments for 'R.BITOP' command" "BITOP ONE with no source bitmaps"

  # Test 2: Single bitmap
  rcall_assert "R.BITOP ONE test_bitop_one64_result_single test_bitop_one64_key1" "ERR wrong number of arguments for 'R.BITOP' command" "BITOP ONE with single bitmap"

  # Test 3: Two non-overlapping bitmaps
  rcall_assert "R.SETINTARRAY test_bitop_one64_key1 1 3 5" "OK" "Set bits in test_bitop_one64_key1"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key2 2 4 6" "OK" "Set bits in test_bitop_one64_key2"

  rcall_assert "R.BITOP ONE test_bitop_one64_result_non_overlap test_bitop_one64_key1 test_bitop_one64_key2" "6" "BITOP ONE with non-overlapping bitmaps"
  rcall_assert "R.GETINTARRAY test_bitop_one64_result_non_overlap" "1\n2\n3\n4\n5\n6" "Result should contain all bits from both keys"

  # Test 4: Two overlapping bitmaps
  rcall_assert "R.SETINTARRAY test_bitop_one64_key3 1 2 3" "OK" "Set bits in test_bitop_one64_key3"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key4 3 4 5" "OK" "Set bits in test_bitop_one64_key4"

  rcall_assert "R.BITOP ONE test_bitop_one64_result_overlap test_bitop_one64_key3 test_bitop_one64_key4" "4" "BITOP ONE with overlapping bitmaps"
  rcall_assert "R.GETINTARRAY test_bitop_one64_result_overlap" "1\n2\n4\n5" "Result should contain only non-overlapping bits"

  # Test 5: Three bitmaps - Redis example
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_a 0 4 5 6" "OK" "Set bits in test_bitop_one64_key_a"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_b 1 5 6" "OK" "Set bits in test_bitop_one64_key_b"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_c 2 3 5 6 7" "OK" "Set bits in test_bitop_one64_key_c"
  
  rcall_assert "R.BITOP ONE test_bitop_one64_result_three test_bitop_one64_key_a test_bitop_one64_key_b test_bitop_one64_key_c" "6" "BITOP ONE with three bitmaps"
  rcall_assert "R.GETINTARRAY test_bitop_one64_result_three" "0\n1\n2\n3\n4\n7" "Result should contain bits appearing exactly once"

  # Test 6: All bitmaps have same bits
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_same1 10 20 30" "OK" "Set bits in test_bitop_one64_key_same1"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_same2 10 20 30" "OK" "Set bits in test_bitop_one64_key_same2"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_same3 10 20 30" "OK" "Set bits in test_bitop_one64_key_same3"

  rcall_assert "R.BITOP ONE test_bitop_one64_result_same test_bitop_one64_key_same1 test_bitop_one64_key_same2 test_bitop_one64_key_same3" "0" "BITOP ONE with identical bitmaps"
  rcall_assert "R.GETINTARRAY test_bitop_one64_result_same" "" "Result should be empty array"

  # Test 7: One empty bitmap among others
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_with_bits1 1 2 3" "OK" "Set bits in test_bitop_one64_key_with_bits1"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_with_bits2 3 4 5" "OK" "Set bits in test_bitop_one64_key_with_bits2"

  rcall "DEL test_bitop_one64_key_empty"
  rcall_assert "R.BITOP ONE test_bitop_one64_result_mixed test_bitop_one64_key_with_bits1 test_bitop_one64_key_empty test_bitop_one64_key_with_bits2" "4" "BITOP ONE with one empty bitmap"
  rcall_assert "R.GETINTARRAY test_bitop_one64_result_mixed" "1\n2\n4\n5" "Result should contain non-overlapping bits"

  # Test 8: Complex overlaps with four bitmaps
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_complex1 1 2 3 4 5" "OK" "Set bits in test_bitop_one64_key_complex1"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_complex2 2 3 4 6 7" "OK" "Set bits in test_bitop_one64_key_complex2"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_complex3 3 4 5 7 8" "OK" "Set bits in test_bitop_one64_key_complex3"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_complex4 4 5 6 8 9" "OK" "Set bits in test_bitop_one64_key_complex4"

  rcall_assert "R.BITOP ONE test_bitop_one64_result_complex test_bitop_one64_key_complex1 test_bitop_one64_key_complex2 test_bitop_one64_key_complex3 test_bitop_one64_key_complex4" "2" "BITOP ONE with complex overlaps"
  rcall_assert "R.GETINTARRAY test_bitop_one64_result_complex" "1\n9" "Result should contain only bits appearing exactly once"

  # Test 9: Large bit values
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_large1 1000000 2000000" "OK" "Set large bits in key_large1"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_large2 2000000 3000000" "OK" "Set large bits in key_large2"

  rcall_assert "R.BITOP ONE test_bitop_one64_result_large test_bitop_one64_key_large1 test_bitop_one64_key_large2" "2" "BITOP ONE with large bit values"
  rcall_assert "R.GETINTARRAY test_bitop_one64_result_large" "1000000\n3000000" "Result should contain non-overlapping large bits"

  # Test 10: Destination already has content
  rcall_assert "R.SETINTARRAY test_bitop_one64_result_preexist 100 200 300" "OK" "Set bits in test_bitop_one64_result_preexist"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_overwrite1 1 2" "OK" "Set bits in test_bitop_one64_key_overwrite1"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_overwrite2 2 3" "OK" "Set bits in test_bitop_one64_key_overwrite2"

  rcall_assert "R.BITOP ONE test_bitop_one64_result_preexist test_bitop_one64_key_overwrite1 test_bitop_one64_key_overwrite2" "2" "BITOP ONE should overwrite destination"
  rcall_assert "R.GETINTARRAY test_bitop_one64_result_preexist" "1\n3" "Result should only contain new operation result"

  # Test 11: Input bitmaps should remain unchanged
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_immutable1 10 20 30" "OK" "Set bits in test_bitop_one64_key_immutable1"
  rcall_assert "R.SETINTARRAY test_bitop_one64_key_immutable2 20 30 40" "OK" "Set bits in test_bitop_one64_key_immutable2"

  rcall_assert "R.BITOP ONE test_bitop_one64_result_immutable test_bitop_one64_key_immutable1 test_bitop_one64_key_immutable2" "2" "BITOP ONE operation"
  rcall_assert "R.GETINTARRAY test_bitop_one64_key_immutable1" "10\n20\n30" "key_immutable1 should remain unchanged"
  rcall_assert "R.GETINTARRAY test_bitop_one64_key_immutable2" "20\n30\n40" "key_immutable2 should remain unchanged"
  rcall_assert "R.GETINTARRAY test_bitop_one64_result_immutable" "10\n40" "Result should contain bits appearing exactly once"
}

function test_diff() {
  print_test_header "test_diff (64)"

  rcall_assert "R64.DIFF test_diff_res" "ERR wrong number of arguments for 'R64.DIFF' command" "DIFF with wrong number of arguments"
  rcall_assert "R64.DIFF test_diff_res empty_key_1 empty_key_2" "WRONGTYPE Operation against a key holding the wrong kind of value" "DIFF with empty keys"

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
  rcall_assert "R64.OPTIMIZE no-key" "ERR no such key" "Optimize non-existent key"
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
test_bitop
test_bitcount
test_bitpos
test_getintarray_setintarray
test_getbitarray_setbitarray
test_appendintarray_deleteintarray
test_setrage
test_clear
test_min_max
test_bitop_one
test_diff
test_optimize_nokey
# test_setfull
test_del
test_stat
test_save
