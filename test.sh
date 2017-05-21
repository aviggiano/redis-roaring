#!/usr/bin/env bash

set -eu

LOG_FILE=$(mktemp)
function unit()
{
  mkdir -p build
  cd build
  TEST=1 cmake ..
  make
  valgrind --leak-check=full --error-exitcode=1 ./test_redis-roaring
  cd -
}
function build_redis_module()
{
  mkdir -p build
  cd build
  cmake ..
  make
  cd -
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
function run_integration_tests()
{
  ./tests/integration.sh
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
  build_redis_module
  start_redis
  run_integration_tests
  stop_redis
}

unit
echo "All unit tests passed"
integration
echo "All integration tests passed"