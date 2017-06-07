#!/usr/bin/env bash

function configure_croaring()
{
  pushd .

  git submodule init
  git submodule update

  cd deps/CRoaring

  # generates header files
  ./amalgamation.sh

  # https://github.com/RoaringBitmap/CRoaring#building-with-cmake-linux-and-macos-visual-studio-users-should-see-below
  mkdir -p build
  cd build
  cmake -DBUILD_STATIC=ON ..
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

configure_croaring
configure_redis
configure_hiredis