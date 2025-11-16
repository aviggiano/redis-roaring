#!/usr/bin/env bash

set -eu

. ./tests/helper.sh

function performance()
{
  stop_redis
  start_redis

  # Run performance tests and capture output
  PERF_OUTPUT=$(./build/performance | grep -E '^\|')

  stop_redis
  echo "All performance tests passed"

  # Update README.md with the latest performance results
  if [ -f "README.md" ] && [ ! -z "$PERF_OUTPUT" ]; then
    echo "Updating README.md with latest performance results..."

    # Create a temporary file with the new content
    awk -v perf="$PERF_OUTPUT" '
      /<!-- BEGIN_PERFORMANCE -->/ {
        print
        print perf
        skip=1
        next
      }
      /<!-- END_PERFORMANCE -->/ {
        skip=0
      }
      !skip
    ' README.md > README.md.tmp

    mv README.md.tmp README.md
    echo "README.md updated successfully"
  fi
}

setup
performance
