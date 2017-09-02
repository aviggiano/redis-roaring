#!/usr/bin/env bash

set -eux

function test_load()
{
  echo "test_load"

  FOUND=$(echo "KEYS *" | redis-cli)
  EXPECTED="test_"
  [[ "$FOUND" =~ .*"$EXPECTED".* ]]
}

test_load
