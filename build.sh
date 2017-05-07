#!/usr/bin/env bash

# builds CRoaring
pushd .
cd deps/CRoaring/
mkdir -p build
cd build
cmake ..
make

# builds reroaring
popd
mkdir -p build
cd build
cmake ..
make