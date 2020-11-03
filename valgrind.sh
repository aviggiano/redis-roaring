#!/usr/bin/env bash

set -eux

CURDIR=$(pwd)

cd /tmp
wget ftp://sourceware.org/pub/valgrind/valgrind-3.13.0.tar.bz2
tar -xjf valgrind-3.13.0.tar.bz2
cd valgrind-3.13.0
./configure --prefix=/usr/local
make
sudo make install
ccache --clear
valgrind --version

cd $CURDIR
