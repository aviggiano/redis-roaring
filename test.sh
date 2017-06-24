#!/usr/bin/env bash

set -eu

LOG_FILE=""
function setup()
{
  mkdir -p build
  cd build
  cmake ..
  make
  cd -
}
function unit()
{
  valgrind --leak-check=full --error-exitcode=1 ./build/unit
  echo "All unit tests passed"
}
function start_redis()
{
  local USE_VALGRIND="$1"
  if [ "$USE_VALGRIND" == "yes" ]; then
    LOG_FILE=$(mktemp)
    valgrind --leak-check=yes --show-leak-kinds=definite,indirect --error-exitcode=1 --log-file=$LOG_FILE ./deps/redis/src/redis-server --loadmodule ./build/libredis-roaring.so &
  else
    ./deps/redis/src/redis-server --loadmodule ./build/libredis-roaring.so &
  fi
  while [ "$(./deps/redis/src/redis-cli PING 2>/dev/null)" != "PONG" ]; do
    sleep 0.1
  done
}
function stop_redis()
{
  pkill -f redis || true
  while [ $(ps aux | grep redis | grep -v grep | wc -l) -ne 0 ]; do
    sleep 0.1
  done
  sleep 2
  if [ "$LOG_FILE" != "" ]; then
    cat "$LOG_FILE"
    local VALGRIND_ERRORS=$(cat "$LOG_FILE" | grep --color=no "indirectly lost\|definitely lost\|Invalid write\|Invalid read\|uninitialised\|Invalid free\|a block of size" | grep --color=no -v ": 0 bytes in 0 blocks")
    [ "$VALGRIND_ERRORS" == "" ]
    rm "$LOG_FILE"
    LOG_FILE=""
  fi
}
function integration()
{
  stop_redis
  start_redis "yes"
  ./tests/integration.sh
  stop_redis
  echo "All integration tests passed"
}
function performance()
{
  stop_redis
  start_redis "no"
  ./build/performance
  stop_redis
  echo "All performance tests passed"
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
end