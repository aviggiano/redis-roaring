#!/usr/bin/env bash

function configure_submodules()
{
  git submodule init
  git submodule update
  git submodule status
}
function configure_croaring()
{
  pushd .

  cd deps/CRoaring

  # generates header files
  ./amalgamation.sh

  # https://github.com/RoaringBitmap/CRoaring#building-with-cmake-linux-and-macos-visual-studio-users-should-see-below
#  rm -rf  build
  mkdir -p build
  cd build
  cmake -DBUILD_STATIC=ON -DCMAKE_BUILD_TYPE=Debug ..
  make

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

configure_submodules
configure_croaring
configure_redis
configure_hiredis

mkdir -p build && cd build
cmake ..
make

cd .. && mkdir -p dist
cp build/libredis-roaring.dylib dist
cp deps/redis/redis.conf dist
cp deps/redis/src/{redis-benchmark,redis-check-aof,redis-check-rdb,redis-cli,redis-sentinel,redis-server} dist
echo "loadmodule `pwd`/dist/libredis-roaring.dylib" >> dist/redis.conf
echo "so you can:"
echo "cd dist" 
echo "start server:" 
echo "./redis-server ./redis.conf"  
echo "connect to server:"
echo "./redis-cli"
echo "and test these commands"
