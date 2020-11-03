#!/usr/bin/env bash

set -eu

. helper.sh

function performance()
{
  stop_redis
  start_redis
  ./build/performance
  stop_redis
  echo "All performance tests passed"
}

setup
performance
end
