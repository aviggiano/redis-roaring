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
  rm dump.rdb 2>/dev/null || true
  rm appendonly.aof 2>/dev/null || true
}
function start_redis()
{
  local USE_VALGRIND="no"
  local USE_AOF="no"
  while [[ $# -gt 0 ]]; do
    local PARAM="$1"
    case $PARAM in
      --valgrind)
        USE_VALGRIND="yes"
        LOG_FILE=$(mktemp)
        ;;
      --aof)
        USE_AOF="yes"
        ;;
    esac
    shift
  done

  local REDIS_COMMAND="./deps/redis/src/redis-server --loadmodule ./build/libredis-roaring.so"
  local VALGRIND_COMMAND="valgrind --leak-check=yes --show-leak-kinds=definite,indirect --suppressions=./deps/redis/src/valgrind.sup --error-exitcode=1 --log-file=$LOG_FILE"
  local AOF_OPTION="--appendonly $USE_AOF"
  if [ "$USE_VALGRIND" == "no" ]; then
    VALGRIND_COMMAND=""
  fi

  eval "$VALGRIND_COMMAND" "$REDIS_COMMAND" "$AOF_OPTION" &

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
function end()
{
  echo ""
  echo "************************"
  echo "*** ALL TESTS PASSED ***"
  echo "************************"
  echo ""
}
