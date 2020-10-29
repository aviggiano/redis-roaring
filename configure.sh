#!/usr/bin/env bash

function configure_submodules()
{
  git submodule init
  git submodule update
  git submodule status
}
function amalgamate_croaring()
{
  pushd . 
  cd src 
  # generates header files
  ../deps/CRoaring/amalgamation.sh 
  cp ../deps/CRoaring/roaring.c .
  cp ../deps/CRoaring/roaring.h .
  rm roaring.hh && rm amalgamation_demo.c && rm amalgamation_demo.cpp
  popd
} 
function configure_redis()
{
  cd deps/redis
  make
  cd -
}
function configure_hiredis()
{
  cd deps/hiredis
  make
  cd -
}
function build()
{
  mkdir -p build
  cd build
  cmake ..
  make
  local LIB=$(find libredis-roaring*)
  
  cd ..
  mkdir -p dist
  cp "build/$LIB" dist
  cp deps/redis/redis.conf dist
  cp deps/redis/src/{redis-benchmark,redis-check-aof,redis-check-rdb,redis-cli,redis-sentinel,redis-server} dist
  echo "loadmodule $(pwd)/dist/$LIB" >> dist/redis.conf
}
function instructions()
{
  echo ""
  echo "Start redis server with redis-roaring:"
  echo "./dist/redis-server ./dist/redis.conf"  
  echo "Connect to server:"
  echo "./dist/redis-cli"
}

configure_submodules
amalgamate_croaring
configure_redis
configure_hiredis
build
instructions

