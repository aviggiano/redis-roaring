#!/usr/bin/env bash

set -eu

. ./tests/helper.sh

function unit() {
  if [[ "${USE_VALGRIND:-1}" != "1" ]] || [[ "$OSTYPE" == "darwin"* ]]; then
    ./build/unit
  else
    valgrind --leak-check=full --error-exitcode=1 ./build/unit
  fi

  echo "All unit tests passed"
}

function integration_1() {
  stop_redis
  rm dump.rdb 2>/dev/null || true
  if [[ "${USE_VALGRIND:-1}" == "1" ]]; then
    start_redis --valgrind
  else
    start_redis
  fi
  ./tests/integration_1.sh
  stop_redis
  echo "All integration (1) tests passed"
}

function integration_2() {
  stop_redis
  if [[ "${USE_VALGRIND:-1}" == "1" ]]; then
    start_redis --valgrind --aof
  else
    start_redis --aof
  fi
  ./tests/integration_1.sh
  stop_redis

  # Test RDB load
  if [[ "${USE_VALGRIND:-1}" == "1" ]]; then
    start_redis --valgrind
  else
    start_redis
  fi
  ./tests/integration_2.sh
  stop_redis
  rm dump.rdb 2>/dev/null || true

  # Test AOF load
  if [[ "${USE_VALGRIND:-1}" == "1" ]]; then
    start_redis --valgrind --aof
  else
    start_redis --aof
  fi
  ./tests/integration_2.sh
  stop_redis
  rm appendonly.aof 2>/dev/null || true

  echo "All integration tests passed"
}

function integration_3() {
  stop_redis
  rm dump.rdb 2>/dev/null || true
  if [[ "${USE_VALGRIND:-1}" == "1" ]]; then
    start_redis --valgrind
  else
    start_redis
  fi
  ./tests/integration_3.sh
  stop_redis
  echo "All integration (3) tests passed"
}

setup
unit
integration_1
integration_2
integration_3

echo ""
echo "************************"
echo "*** ALL TESTS PASSED ***"
echo "************************"
echo ""
