#!/usr/bin/env bash

set -eu

. "$(dirname "$0")/helper.sh"

function test_bitop_cluster_regression() {
  print_test_header "test_bitop_cluster_regression"

  if ! ./deps/redis/src/redis-cli -p "$REDIS_PORT" CLUSTER INFO 2>/dev/null | grep -q '^cluster_state:ok'; then
    addslots_result=$(assign_all_cluster_slots)
    if [ "$addslots_result" = "OK" ]; then
      echo -e "\x1b[32m✓\x1b[0m Assign all cluster slots"
    else
      echo -e "\x1b[31m✗\x1b[0m Assign all cluster slots"
      echo "  Got: '$addslots_result'"
      return 1
    fi
  else
    echo -e "\x1b[32m✓\x1b[0m Cluster already has all slots assigned"
  fi

  wait_for_cluster_ok

  rcall_assert "R.SETBIT {bitop}foo 1 1" "0" "Seed clustered 32-bit source bitmap foo"
  rcall_assert "R.SETBIT {bitop}bar 2 1" "0" "Seed clustered 32-bit source bitmap bar"
  rcall_assert "R.BITOP OR {bitop}dest {bitop}foo {bitop}bar" "2" "Clustered R.BITOP OR should succeed"
  rcall_assert "R.GETBIT {bitop}dest 1" "1" "Clustered OR result should keep bit 1"
  rcall_assert "R.GETBIT {bitop}dest 2" "1" "Clustered OR result should keep bit 2"
  rcall_assert "R.BITOP NOT {bitop}not {bitop}foo" "1" "Clustered R.BITOP NOT should succeed"

  rcall_assert "R64.SETBIT {bitop64}foo 1 1" "0" "Seed clustered 64-bit source bitmap foo"
  rcall_assert "R64.SETBIT {bitop64}bar 2 1" "0" "Seed clustered 64-bit source bitmap bar"
  rcall_assert "R64.BITOP OR {bitop64}dest {bitop64}foo {bitop64}bar" "2" "Clustered R64.BITOP OR should succeed"
  rcall_assert "R64.GETBIT {bitop64}dest 1" "1" "Clustered 64-bit OR result should keep bit 1"
  rcall_assert "R64.GETBIT {bitop64}dest 2" "1" "Clustered 64-bit OR result should keep bit 2"
  rcall_assert "R64.BITOP NOT {bitop64}not {bitop64}foo" "2" "Clustered R64.BITOP NOT should succeed"

  rcall_assert "PING" "PONG" "Redis should stay alive after clustered BITOP operations"
}

test_bitop_cluster_regression
