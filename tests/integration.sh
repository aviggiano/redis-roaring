#!/usr/bin/env bash

set -eu

function test_rsetbit_rgetbit()
{
  echo "test_rsetbit_rgetbit"
  EXPECTED=0
  FOUND=$(echo "R.SETBIT key 0 1" | redis-cli)
  [ $FOUND -eq $EXPECTED ]
  EXPECTED=1
  FOUND=$(echo "R.GETBIT key 0" | redis-cli)
  [ $FOUND -eq $EXPECTED ]
}
function test_done()
{
  echo "All integration tests passed"
}

test_rsetbit_rgetbit
test_done