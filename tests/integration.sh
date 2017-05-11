#!/usr/bin/env bash

set -eu

function rsetbit_rgetbit()
{
  EXPECTED=0
  FOUND=$(echo "R.SETBIT key 0 1" | redis-cli)
  [ $FOUND -eq $EXPECTED ]
  EXPECTED=1
  FOUND=$(echo "R.GETBIT key 0" | redis-cli)
  [ $FOUND -eq $EXPECTED ]
}

rsetbit_rgetbit