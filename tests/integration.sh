#!/usr/bin/env bash

function rsetbit_rgetbit()
{
  echo "R.SETBIT key 0 1" | redis-cli
  EXPECTED=1
  FOUND=$(echo "R.GETBIT key 0" | redis-cli)
  [ $FOUND -eq $EXPECTED ]
}

rsetbit_rgetbit