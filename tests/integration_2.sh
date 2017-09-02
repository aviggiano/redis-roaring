#!/usr/bin/env bash

set -eu

function test_load()
{
  echo "test_load"

  FOUND=$(echo "KEYS *" | redis-cli)
  EXPECTED="test_"
  [[ "$FOUND" =~ .*"$EXPECTED".* ]]
}

test_load
