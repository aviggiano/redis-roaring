#!/usr/bin/env bash

set -eu

function test_rsetbit_rgetbit()
{
  echo "test_rsetbit_rgetbit"
  EXPECTED=0
  FOUND=$(echo "R.SETBIT test_rsetbit_rgetbit 0 1" | redis-cli)
  [ $FOUND -eq $EXPECTED ]
  EXPECTED=1
  FOUND=$(echo "R.GETBIT test_rsetbit_rgetbit 0" | redis-cli)
  [ $FOUND -eq $EXPECTED ]
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
function test_done()
{
  echo "All integration tests passed"
}

test_rsetbit_rgetbit
test_rsetbit_rgetbit_wrong_arity
test_done