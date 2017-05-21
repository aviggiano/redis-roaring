#!/usr/bin/env bash

set -eu

function test_rsetbit_rgetbit()
{
  echo "test_rsetbit_rgetbit"
  EXPECTED="0"
  FOUND=$(echo "R.SETBIT test_rsetbit_rgetbit 0 1" | redis-cli)
  [ "$FOUND" == "$EXPECTED" ]
  EXPECTED="1"
  FOUND=$(echo "R.GETBIT test_rsetbit_rgetbit 0" | redis-cli)
  [ "$FOUND" == "$EXPECTED" ]
}
function test_rsetbit_rgetbit_wrong_arity()
{
  echo "test_rsetbit_rgetbit_wrong_arity"
  EXPECTED="ERR wrong number of arguments for 'R.SETBIT' command"
  FOUND=$(echo "R.SETBIT test_rsetbit_rgetbit_wrong_arity 0" | redis-cli)
  [ "$FOUND" == "$EXPECTED" ]
  EXPECTED="ERR wrong number of arguments for 'R.GETBIT' command"
  FOUND=$(echo "R.GETBIT key 0 1" | redis-cli)
  [ "$FOUND" == "$EXPECTED" ]
}
function test_rbitop()
{
  echo "test_rbitop"
  function wrong_arity()
  {
    EXPECTED="ERR syntax error"
    FOUND=$(echo "R.BITOP test_rbitop_wrong_arity NOOP srckey1 srckey2" | redis-cli)
    [ "$FOUND" == "$EXPECTED" ]
  }
  function setup()
  {
    FOUND=$(echo "R.SETBIT test_rbitop_4 1 1" | redis-cli)
    FOUND=$(echo "R.SETBIT test_rbitop_4 2 1" | redis-cli)
    FOUND=$(echo "R.SETBIT test_rbitop_2 1 1" | redis-cli)
  }

  function and()
  {
    FOUND=$(echo "R.BITOP AND test_rbitop_dest_2 test_rbitop_2 test_rbitop_4" | redis-cli)
    EXPECTED="1"
    [ "$FOUND" == "$EXPECTED" ]
    FOUND=$(echo "R.GETBIT test_rbitop_dest_2 1" | redis-cli)
    EXPECTED="1"
    [ "$FOUND" == "$EXPECTED" ]
  }

  function or()
  {
    FOUND=$(echo "R.BITOP OR test_rbitop_dest_6 test_rbitop_2 test_rbitop_4" | redis-cli)
    EXPECTED="2"
    [ "$FOUND" == "$EXPECTED" ]
    FOUND=$(echo "R.GETBIT test_rbitop_dest_6 1" | redis-cli)
    EXPECTED="1"
    [ "$FOUND" == "$EXPECTED" ]
    FOUND=$(echo "R.GETBIT test_rbitop_dest_6 2" | redis-cli)
    EXPECTED="1"
    [ "$FOUND" == "$EXPECTED" ]
  }

  function xor()
  {
    FOUND=$(echo "R.BITOP XOR test_rbitop_dest_4 test_rbitop_2 test_rbitop_4" | redis-cli)
    EXPECTED="1"
    [ "$FOUND" == "$EXPECTED" ]
    FOUND=$(echo "R.GETBIT test_rbitop_dest_4 2" | redis-cli)
    EXPECTED="1"
    [ "$FOUND" == "$EXPECTED" ]
  }

  function not()
  {
    FOUND=$(echo "R.BITOP NOT test_rbitop_dest_3 test_rbitop_4" | redis-cli)
    EXPECTED="2"
    [ "$FOUND" == "$EXPECTED" ]
    FOUND=$(echo "R.GETBIT test_rbitop_dest_3 0" | redis-cli)
    EXPECTED="1"
    [ "$FOUND" == "$EXPECTED" ]
    FOUND=$(echo "R.GETBIT test_rbitop_dest_3 1" | redis-cli)
    EXPECTED="1"
    [ "$FOUND" == "$EXPECTED" ]
  }

  function collision()
  {
    FOUND=$(echo "R.BITOP AND test_rbitop_collision test_rbitop_4 test_rbitop_2" | redis-cli)
    FOUND=$(echo "R.BITOP OR test_rbitop_collision test_rbitop_4 test_rbitop_2" | redis-cli)
  }

  setup
  wrong_arity
  and
  or
  xor
  and
  collision
}
function test_bitcount()
{
  echo "test_bitcount"
  FOUND=$(echo "R.SETBIT test_bitcount 1 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitcount 1 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitcount 2 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitcount 3 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitcount 5 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitcount 8 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitcount 13 1" | redis-cli)
  FOUND=$(echo "R.BITCOUNT test_bitcount" | redis-cli)
  EXPECTED="6"
  [ "$FOUND" == "$EXPECTED" ]
}
function test_bitpos()
{
  echo "test_bitpos"
  FOUND=$(echo "R.SETBIT test_bitpos 3 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitpos 6 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitpos 4 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitpos 12 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_bitpos 10 1" | redis-cli)
  FOUND=$(echo "R.BITPOS test_bitpos" | redis-cli)
  EXPECTED="3"
  [ "$FOUND" == "$EXPECTED" ]
}
function test_getarray_setarray()
{
  echo "test_getarray_setarray"

  FOUND=$(echo "R.SETARRAY test_getarray_setarray 0" | redis-cli)
  EXPECTED="OK"
  [ "$FOUND" == "$EXPECTED" ]
  FOUND=$(echo "R.GETARRAY test_getarray_setarray" | redis-cli)
  EXPECTED="0"
  [ "$FOUND" == "$EXPECTED" ]

  FOUND=$(echo "R.SETARRAY test_getarray_setarray 1 10 100 1000 10000 100000" | redis-cli)
  EXPECTED="OK"
  [ "$FOUND" == "$EXPECTED" ]
  FOUND=$(echo "R.GETARRAY test_getarray_setarray" | redis-cli)
  EXPECTED=$(echo -e "1\n10\n100\n1000\n10000\n100000")
  [ "$FOUND" == "$EXPECTED" ]

  FOUND=$(echo "R.SETARRAY test_getarray_setarray 2 4 8 16 32 64 128" | redis-cli)
  EXPECTED="OK"
  [ "$FOUND" == "$EXPECTED" ]
  FOUND=$(echo "R.GETARRAY test_getarray_setarray" | redis-cli)
  EXPECTED=$(echo -e "2\n4\n8\n16\n32\n64\n128")
  [ "$FOUND" == "$EXPECTED" ]

  FOUND=$(echo "R.GETARRAY test_getarray_setarray_empty_key" | redis-cli)
  EXPECTED=""
  [ "$FOUND" == "$EXPECTED" ]
}

test_rsetbit_rgetbit
test_rsetbit_rgetbit_wrong_arity
test_rbitop
test_bitcount
test_bitpos
test_getarray_setarray