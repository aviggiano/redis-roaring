#!/usr/bin/env bash

set -eu

. "$(dirname "$0")/helper.sh"

# Exercises the module's aof_rewrite callbacks. The AOF command log alone does
# not reach them: they only run when the server rewrites the AOF without an RDB
# preamble, so the caller must start Redis with --aof-use-rdb-preamble no.
#
# Run as `integration_5.sh seed` before a restart and `integration_5.sh verify`
# after it.

BIG_COUNT=1200

# Prints the AOF the last rewrite produced: appendonlydir/*.base.aof on Redis 7+,
# appendonly.aof on 6.2.
function cat_rewritten_aof() {
  if compgen -G "./appendonlydir/*.base.aof" > /dev/null; then
    cat ./appendonlydir/*.base.aof
  else
    cat ./appendonly.aof
  fi
}

function seed_bitmaps() {
  print_test_header "test_aof_rewrite_seed"

  # Without this the rewrite stores module values through the RDB serializer and
  # the aof_rewrite callbacks never run — every assertion below would then pass
  # while testing nothing.
  rcall_assert "CONFIG GET aof-use-rdb-preamble" "aof-use-rdb-preamble\nno" "AOF rewrites use the module callbacks"

  rcall_assert "R.SETINTARRAY test_aof_r32 1 2 5 100 65536" "OK" "Seed a 32 bit bitmap"
  rcall_assert "R64.SETINTARRAY test_aof_r64 1 2 5 100 65536 18446744073709551615" "OK" "Seed a 64 bit bitmap up to the 64 bit maximum"

  # Emptied bitmaps: the key still exists and must survive the rewrite.
  rcall_assert "R.SETINTARRAY test_aof_r32_empty 7" "OK" "Seed a 32 bit bitmap to empty out"
  rcall_assert "R.SETBIT test_aof_r32_empty 7 0" "1" "Clear the only bit of the 32 bit bitmap"
  rcall_assert "R.BITCOUNT test_aof_r32_empty" "0" "32 bit bitmap is empty before the rewrite"

  rcall_assert "R64.SETINTARRAY test_aof_r64_empty 7" "OK" "Seed a 64 bit bitmap to empty out"
  rcall_assert "R64.SETBIT test_aof_r64_empty 7 0" "1" "Clear the only bit of the 64 bit bitmap"
  rcall_assert "R64.BITCOUNT test_aof_r64_empty" "0" "64 bit bitmap is empty before the rewrite"

  # More values than fit a single emitted command, to cover chunked rewrites.
  local values=""
  for ((i = 0; i < BIG_COUNT; i++)); do
    values="$values $((i * 1000))"
  done
  rcall_assert "R64.SETINTARRAY test_aof_r64_big$values" "OK" "Seed a 64 bit bitmap larger than one rewrite chunk"
  rcall_assert "R64.BITCOUNT test_aof_r64_big" "$BIG_COUNT" "Large 64 bit bitmap has every value before the rewrite"

  rcall_assert "R.SETINTARRAY test_aof_r32_big$values" "OK" "Seed a 32 bit bitmap larger than one rewrite chunk"
  rcall_assert "R.BITCOUNT test_aof_r32_big" "$BIG_COUNT" "Large 32 bit bitmap has every value before the rewrite"

  rcall_assert "DBSIZE" "6" "Only the seeded keys exist before the rewrite"

  rewrite_aof
}

function rewrite_aof() {
  print_test_header "test_aof_rewrite_run"

  rcall_assert "BGREWRITEAOF" "Background append only file rewriting started" "Start the AOF rewrite"

  local tries=0
  local info=""
  while true; do
    info=$(./deps/redis/src/redis-cli -p "$REDIS_PORT" INFO persistence | tr -d '\r')
    if echo "$info" | grep -q '^aof_rewrite_in_progress:0'; then
      break
    fi
    tries=$((tries + 1))
    if [ "$tries" -ge 300 ]; then
      echo "AOF rewrite did not finish" >&2
      return 1
    fi
    sleep 0.1
  done

  # Reaching progress zero is not enough: a failed rewrite leaves the previous
  # AOF in place, so the reload below would replay the seed commands and pass
  # without ever loading the module's rewrite output.
  local status
  status=$(echo "$info" | grep '^aof_last_bgrewrite_status:' | cut -d: -f2)
  if [ "$status" != "ok" ]; then
    echo "AOF rewrite failed: $status" >&2
    return 1
  fi

  rcall_assert "PING" "PONG" "Server is responsive after the rewrite"

  # The chunked emission is what turns a huge bitmap into a handful of commands;
  # assert it rather than trusting that BIG_COUNT still exceeds the chunk size.
  local aof
  aof=$(cat_rewritten_aof | tr -d '\r')
  if ! echo "$aof" | grep -q "R64.APPENDINTARRAY"; then
    echo "The rewritten AOF has no R64.APPENDINTARRAY: the 64 bit bitmap was not chunked" >&2
    return 1
  fi
  if ! echo "$aof" | grep -q "R.APPENDINTARRAY"; then
    echo "The rewritten AOF has no R.APPENDINTARRAY: the 32 bit bitmap was not chunked" >&2
    return 1
  fi
  echo -e "\x1b[32m✓\x1b[0m Both bitmaps were emitted in chunks"
}

function verify_bitmaps() {
  print_test_header "test_aof_rewrite_verify"

  rcall_assert "R.BITCOUNT test_aof_r32" "5" "32 bit bitmap keeps its cardinality"
  rcall_assert "R.GETINTARRAY test_aof_r32" "1\n2\n5\n100\n65536" "32 bit bitmap keeps its values"

  rcall_assert "R64.BITCOUNT test_aof_r64" "6" "64 bit bitmap keeps its cardinality"
  rcall_assert "R64.GETINTARRAY test_aof_r64" "1\n2\n5\n100\n65536\n18446744073709551615" "64 bit bitmap keeps its values, including the 64 bit maximum"

  rcall_assert "EXISTS test_aof_r32_empty" "1" "Empty 32 bit bitmap survives the rewrite"
  rcall_assert "R.BITCOUNT test_aof_r32_empty" "0" "Empty 32 bit bitmap is still empty"
  rcall_assert "R.GETBIT test_aof_r32_empty 7" "0" "Empty 32 bit bitmap did not regain its cleared bit"

  rcall_assert "EXISTS test_aof_r64_empty" "1" "Empty 64 bit bitmap survives the rewrite"
  rcall_assert "R64.BITCOUNT test_aof_r64_empty" "0" "Empty 64 bit bitmap is still empty"
  rcall_assert "R64.GETBIT test_aof_r64_empty 7" "0" "Empty 64 bit bitmap did not regain its cleared bit"

  rcall_assert "R64.BITCOUNT test_aof_r64_big" "$BIG_COUNT" "Chunked 64 bit bitmap keeps every value"
  rcall_assert "R64.GETBIT test_aof_r64_big 0" "1" "Chunked 64 bit bitmap keeps its first value"
  rcall_assert "R64.GETBIT test_aof_r64_big $(((BIG_COUNT - 1) * 1000))" "1" "Chunked 64 bit bitmap keeps its last value"
  rcall_assert "R64.GETBIT test_aof_r64_big 1" "0" "Chunked 64 bit bitmap gained no extra values"

  rcall_assert "R.BITCOUNT test_aof_r32_big" "$BIG_COUNT" "Chunked 32 bit bitmap keeps every value"
  rcall_assert "R.GETBIT test_aof_r32_big 0" "1" "Chunked 32 bit bitmap keeps its first value"
  rcall_assert "R.GETBIT test_aof_r32_big $(((BIG_COUNT - 1) * 1000))" "1" "Chunked 32 bit bitmap keeps its last value"
  rcall_assert "R.GETBIT test_aof_r32_big 1" "0" "Chunked 32 bit bitmap gained no extra values"

  rcall_assert "DBSIZE" "6" "The rewrite restored exactly the seeded keys"
}

case "${1:-}" in
  seed)
    seed_bitmaps
    ;;
  verify)
    verify_bitmaps
    ;;
  *)
    echo "usage: $0 {seed|verify}" >&2
    exit 1
    ;;
esac
