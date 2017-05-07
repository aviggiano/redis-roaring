#!/usr/bin/env bash

cd deps/CRoaring/
mkdir -p build
cd build
cmake ..
make

