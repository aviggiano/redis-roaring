#!/usr/bin/env bash

set -eu

LOG_FILE="${LOG_FILE:-}"
REDIS_PID=""
REDIS_PORT="${REDIS_PORT:-}"
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

  if [ "$REDIS_PORT" == "" ]; then
    for port in 6379 6380 6381 6382 6383; do
      if ./deps/redis/src/redis-cli -p "$port" PING >/dev/null 2>&1; then
        continue
      fi
      REDIS_PORT="$port"
      break
    done
  fi

  if [ "$REDIS_PORT" == "" ]; then
    echo "No available Redis port found for tests" >&2
    exit 1
  fi
  export REDIS_PORT

  local REDIS_COMMAND="./deps/redis/src/redis-server --loglevel warning --loadmodule $LIB_PATH --port $REDIS_PORT"
  local VALGRIND_COMMAND="valgrind --leak-check=yes --show-leak-kinds=definite,indirect --suppressions=./deps/redis/src/valgrind.sup --error-exitcode=1 --log-file=$LOG_FILE"
  local AOF_OPTION="--appendonly $USE_AOF"
  if [ "$USE_VALGRIND" == "no" ]; then
    VALGRIND_COMMAND=""
  fi

  eval "$VALGRIND_COMMAND" "$REDIS_COMMAND" "$AOF_OPTION" &
  REDIS_PID=$!

  while [ "$(./deps/redis/src/redis-cli -p "$REDIS_PORT" PING 2>/dev/null)" != "PONG" ]; do
    sleep 0.1
  done
}

function stop_redis() {
  if [ "$REDIS_PORT" != "" ] && [ "$(./deps/redis/src/redis-cli -p "$REDIS_PORT" PING 2>/dev/null)" == "PONG" ]; then
    ./deps/redis/src/redis-cli -p "$REDIS_PORT" shutdown nosave >/dev/null 2>&1 || true
  fi

  if [ "$REDIS_PID" != "" ]; then
    if kill -0 "$REDIS_PID" 2>/dev/null; then
      kill "$REDIS_PID" 2>/dev/null || true
      local tries=0
      while kill -0 "$REDIS_PID" 2>/dev/null; do
        sleep 0.1
        tries=$((tries + 1))
        if [ "$tries" -ge 300 ]; then
          break
        fi
      done
    fi
    REDIS_PID=""
  fi

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
  echo "$cmd" | ./deps/redis/src/redis-cli -p "$REDIS_PORT" >/dev/null
}

function rcall_assert() {
  local cmd="$1"
  local expected="$(echo -e "$2")"
  local description="${3:-Redis command}"

  local result=$(echo "$cmd" | ./deps/redis/src/redis-cli -p "$REDIS_PORT") 

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
