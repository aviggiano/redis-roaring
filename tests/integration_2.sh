#!/usr/bin/env bash

set -eu

. "$(dirname "$0")/helper.sh"

function test_load() {
  print_test_header "test_load"

FOUND=$(echo "KEYS *" | ./deps/redis/src/redis-cli -p "$REDIS_PORT")
  EXPECTED="test_"
  [[ "$FOUND" =~ .*"$EXPECTED".* ]]
}

test_load
