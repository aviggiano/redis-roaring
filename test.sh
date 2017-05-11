#!/usr/bin/env bash

set -eu

# FIXME rm build/CMakeCache.txt

function unit()
{
  mkdir -p build && \
(rm build/CMakeCache.txt 2>/dev/null || true) && \
cd build && \
cmake -DCOMPILE_MODE:STRING="test" .. && \
make && \
valgrind --leak-check=full --error-exitcode=1 ./test_reroaring &&
cd -
}
function build_redis_module()
{
  (rm build/CMakeCache.txt 2>/dev/null || true) && \
./build.sh
}
function start_redis()
{
  pkill -f redis
  sleep 1
  ./deps/redis/src/redis-server --loadmodule ./build/libreroaring.so &
}
function run_tests()
{
  ./tests/integration.sh
}
function integration()
{
  build_redis_module && \
start_redis &&
run_tests
}

unit && integration