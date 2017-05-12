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
  EXPECTED="ERR syntax error"
  FOUND=$(echo "R.BITOP test_rsetbit_rgetbit_wrong_arity NOOP srckey1 srckey2" | redis-cli)
  [ "$FOUND" == "$EXPECTED" ]
  FOUND=$(echo "R.SETBIT test_rsetbit_rgetbit_wrong_arity_4 1 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_rsetbit_rgetbit_wrong_arity_4 2 1" | redis-cli)
  FOUND=$(echo "R.SETBIT test_rsetbit_rgetbit_wrong_arity_2 1 1" | redis-cli)
  FOUND=$(echo "R.BITOP AND test_rsetbit_rgetbit_wrong_arity_dest_2 test_rsetbit_rgetbit_wrong_arity_2 test_rsetbit_rgetbit_wrong_arity_4" | redis-cli)
  EXPECTED="1"
  [ "$FOUND" == "$EXPECTED" ]
  FOUND=$(echo "R.BITOP OR test_rsetbit_rgetbit_wrong_arity_dest_6 test_rsetbit_rgetbit_wrong_arity_2 test_rsetbit_rgetbit_wrong_arity_4" | redis-cli)
  EXPECTED="2"
  [ "$FOUND" == "$EXPECTED" ]
  FOUND=$(echo "R.BITOP XOR test_rsetbit_rgetbit_wrong_arity_dest_2 test_rsetbit_rgetbit_wrong_arity_2 test_rsetbit_rgetbit_wrong_arity_4" | redis-cli)
  EXPECTED="1"
  [ "$FOUND" == "$EXPECTED" ]
  # TODO test dest content 
}
function test_done()
{
  echo "All integration tests passed"
}

test_rsetbit_rgetbit
test_rsetbit_rgetbit_wrong_arity
test_rbitop
test_done