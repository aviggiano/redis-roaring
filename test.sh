#!/usr/bin/env bash

function unit()
{
  mkdir -p build && \
cd build && \
cmake -DCOMPILE_MODE:STRING="test" .. && \
make && \
valgrind --leak-check=full --error-exitcode=1 ./test_reroaring &&
cd -
}
function build_redis()
{
  cd deps/redis
  if [ ! -f src/redis-server ]; then
    make
  fi
  cd -
}
function start_redis()
{
  ./deps/redis/src/redis-server --loadmodule ./build/libreroaring.so &
}
function run_tests()
{
  ./tests/integration.sh
}
function integration()
{
  build_redis
  start_redis
  run_tests
}

unit
integration