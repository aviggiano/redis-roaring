#!/usr/bin/env bash

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