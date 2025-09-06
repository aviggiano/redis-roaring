#!/usr/bin/env bash

set -eu

LOG_FILE=""
function setup() {
  mkdir -p build
  cd build
  cmake ..
  make
  cd -
  rm dump.rdb 2>/dev/null || true
  rm appendonly.aof 2>/dev/null || true
  rm -rf ./appendonlydir 2>/dev/null || true
}

function start_redis() {
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

  local LIB_PATH
  if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    LIB_PATH="./build/libredis-roaring.dylib"
    USE_VALGRIND="no"
  else
    # Linux
    LIB_PATH="./build/libredis-roaring.so"
  fi

  local REDIS_COMMAND="./deps/redis/src/redis-server --loglevel warning --loadmodule $LIB_PATH"
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

function stop_redis() {
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

function rcall() {
  local cmd="$1"
  echo "$cmd" | ./deps/redis/src/redis-cli >/dev/null
}

function rcall_assert() {
  local cmd="$1"
  local expected="$(echo -e "$2")"
  local description="${3:-Redis command}"

  local result=$(echo "$cmd" | ./deps/redis/src/redis-cli) 

  if [ "$result" == "$expected" ]; then
    echo -e "\x1b[32m✓\x1b[0m $description"
  else
    echo -e "\x1b[31m✗\x1b[0m $description"
    echo "  Command: $cmd"
    echo "  Expected: '$expected'"
    echo "  Got: '$result'"
    return 1
  fi
}

function print_test_header() {
  local test_name="$1"
  local total_width=80
  local name_length=${#test_name}
  local padding_total=$((total_width - name_length - 2))  # -2 for spaces around name
  local padding_left=$((padding_total / 2))
  local padding_right=$((padding_total - padding_left))

  repeat_char() {
    local char="$1"
    local count="$2"
    local result=""
    local i=0
    
    while [ $i -lt $count ]; do
      result="${result}${char}"
      i=$((i + 1))
    done
    
    echo "$result"
  }

  # Create separator lines with fallback methods
  local left_sep=$(repeat_char "═" $padding_left)
  local right_sep=$(repeat_char "═" $padding_right)
  
  echo
  echo "${left_sep} ${test_name} ${right_sep}"
  echo
}
