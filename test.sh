#!/usr/bin/env bash

set -eu

LOG_FILE=$(mktemp)
function setup()
{
  mkdir -p build
  cd build
  cmake ..
  make
  cd -
  start_redis
}
function unit()
{
  valgrind --leak-check=full --error-exitcode=1 ./build/unit
  echo "All unit tests passed"
}
function start_redis()
{
  pkill -f redis || true
  while [ $(ps aux | grep redis | grep -v grep | wc -l) -ne 0 ]; do
    sleep 0.1
  done
  valgrind --leak-check=yes --show-leak-kinds=definite,indirect --error-exitcode=1 --log-file="$LOG_FILE" \
./deps/redis/src/redis-server --loadmodule ./build/libredis-roaring.so &
  while [ "$(./deps/redis/src/redis-cli PING 2>/dev/null)" != "PONG" ]; do
    sleep 0.1
  done
}
function stop_redis()
{
  pkill -f redis
  sleep 2
  cat "$LOG_FILE"
  local VALGRIND_ERRORS=$(cat "$LOG_FILE" | grep --color=no "indirectly lost\|definitely lost\|Invalid write\|Invalid read\|uninitialised\|Invalid free\|a block of size" | grep --color=no -v ": 0 bytes in 0 blocks")
  [ "$VALGRIND_ERRORS" == "" ]
}
function integration()
{
  ./tests/integration.sh
  echo "All integration tests passed"
}
function performance()
{
  start_redis
  ./build/performance
  echo "All performance tests passed"
}
function teardown()
{
  stop_redis
}
function end()
{
  echo ""
  echo "************************"
  echo "*** ALL TESTS PASSED ***"
  echo "************************"
  echo ""
}

setup
unit
integration
performance
teardown
end