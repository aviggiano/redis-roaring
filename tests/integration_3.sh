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

  EXPECTED_STAT=$'cardinality: 1\nnumber of containers: 1\nmax value: 100\nmin value: 100
number of array containers: 1\n\tarray container values: 1\n\tarray container bytes: 2
bitset  containers: 0\n\tbitset  container values: 0\n\tbitset  container bytes: 0
run containers: 0\n\trun container values: 0\n\trun container bytes: 0'
  
  rcall_assert "R64.STAT test_stat" "$EXPECTED_STAT" "Get bitmap statistics"
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
test_min_max
test_diff
test_optimize_nokey
# test_setfull
test_del
test_stat
test_save
