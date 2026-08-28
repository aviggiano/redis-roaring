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

function integration_4() {
  stop_redis
  if [[ "${USE_VALGRIND:-1}" == "1" ]]; then
    start_redis --valgrind --cluster
  else
    start_redis --cluster
  fi
  ./tests/integration_4.sh
  stop_redis
  echo "All integration (4) tests passed"
}

function integration_5() {
  stop_redis
  rm dump.rdb 2>/dev/null || true
  # appendonlydir is Redis 7+; appendonly.aof is the 6.2 layout.
  rm -rf ./appendonlydir 2>/dev/null || true
  rm -f appendonly.aof 2>/dev/null || true

  # Seed, rewrite the AOF, then reload it from a restarted server. The rewrite
  # runs without an RDB preamble so the module's aof_rewrite callbacks are used.
  if [[ "${USE_VALGRIND:-1}" == "1" ]]; then
    start_redis --valgrind --aof --no-rdb-preamble
  else
    start_redis --aof --no-rdb-preamble
  fi
  ./tests/integration_5.sh seed
  stop_redis

  if [[ "${USE_VALGRIND:-1}" == "1" ]]; then
    start_redis --valgrind --aof --no-rdb-preamble
  else
    start_redis --aof --no-rdb-preamble
  fi
  ./tests/integration_5.sh verify
  stop_redis
  rm -rf ./appendonlydir 2>/dev/null || true
  rm -f appendonly.aof 2>/dev/null || true

  echo "All integration (5) tests passed"
}

function integration_6() {
  stop_redis
  rm dump.rdb 2>/dev/null || true
  if [[ "${USE_VALGRIND:-1}" == "1" ]]; then
    start_redis --valgrind
  else
    start_redis
  fi
  REDIS_PORT="$REDIS_PORT" python3 ./tests/integration_6.py
  stop_redis
  rm dump.rdb 2>/dev/null || true
  echo "All integration (6) tests passed"
}

setup
unit
integration_1
integration_2
integration_3
integration_4
integration_5
integration_6

echo ""
echo "************************"
echo "*** ALL TESTS PASSED ***"
echo "************************"
echo ""
