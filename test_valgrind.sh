#!/usr/bin/env bash

mkdir -p build && \
cd build && \
cmake -DCOMPILE_MODE:STRING="test" .. && \
make && \
valgrind --leak-check=full --error-exitcode=1 ./test_reroaring