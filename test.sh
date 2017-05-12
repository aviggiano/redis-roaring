#!/usr/bin/env bash

set -eu

function unit()
{
  mkdir -p build
  cd build
  TEST=1 cmake ..
  make
  valgrind --leak-check=full --error-exitcode=1 ./test_reroaring
  cd -
}
function build_redis_module()
{
  ./build.sh
}
function start_redis()
{
  pkill -f redis || true
  while [ $(ps aux | grep redis | grep -v grep | wc -l) -ne 0 ]; do
    sleep 0.1
  done
  ./deps/redis/src/redis-server --loadmodule ./build/libreroaring.so &
  while [ "$(./deps/redis/src/redis-cli PING)" != "PONG" ]; do
    sleep 0.1
  done
}
function run_tests()
{
  ./tests/integration.sh
}
function integration()
{
  build_redis_module
  start_redis
  run_tests
}

unit
integration
