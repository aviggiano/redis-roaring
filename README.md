redis-roaring [![Build Status](https://travis-ci.org/aviggiano/redis-roaring.svg?branch=master)](https://travis-ci.org/aviggiano/redis-roaring)
===========
Roaring Bitmaps for Redis

## Intro

This project uses the [CRoaring](https://github.com/RoaringBitmap/CRoaring) library to implement roaring bitmap commands for Redis.
These commands can have the same performance as redis' native bitmaps for *O(1)* operations and be [up to 8x faster](#performance) for *O(N)*
calls, according to microbenchmarks, while consuming less memory than their uncompressed counterparts (benchmark pending).

Pull requests are welcome.

## why fork?
- 1 add <a href="#newAPI">new APIs</a>  
- 2 add a <a href="#quickstart">quick start</a>

## Dependencies

- CRoaring (bitmap compression library used by this redis module)
- cmake (build tools needed for compiling the source code)
- redis (server needed for integration tests)
- hiredis (redis client library needed for performance tests)

## Getting started

Clone this repository:

```bash
$ git clone https://github.com/aviggiano/redis-roaring.git
$ cd redis-roaring/
```

Run the `configure.sh` script, or manually do the following steps:

```bash
$ git submodule init
$ git submodule update
$ git submodule status
# cd to deps/CRoaring and compile it, following the build instructions on their repository
# cd to deps/redis and compile it
# cd to deps/hiredis and compile it
```

## Build

Build the project with cmake and it will generate the Redis Module shared library `libredis-roaring.so`, together with unit tests and performance tests.

```bash
mkdir -p build
cd build
cmake ..
make
```

## Docker

It is also possible to run this project as a docker container:

```bash
docker build -t redis-roaring
docker run -p 6379:6379 redis-roaring:latest
```

## Tests

Run the `test.sh` script for unit tests, integration tests and performance tests.
The performance tests can take a while, since they run on a real dataset of integer values.

## API

Currently the following operations are supported

- `R.SETBIT` (same as [SETBIT](https://redis.io/commands/setbit))
- `R.GETBIT` (same as [GETBIT](https://redis.io/commands/getbit))
- `R.BITOP` (same as [BITOP](https://redis.io/commands/bitop))
- `R.BITCOUNT` (same as [BITCOUNT](https://redis.io/commands/bitcount) without `start` and `end` parameters)
- `R.BITPOS` (same as [BITPOS](https://redis.io/commands/bitpos) without `start` and `end` parameters)
- `R.SETINTARRAY` (create a roaring bitmap from an integer array)
- `R.GETINTARRAY` (get an integer array from a roaring bitmap)
- `R.SETBITARRAY` (create a roaring bitmap from a bit array string)
- `R.GETBITARRAY` (get a bit array string from a roaring bitmap)

<a name="newAPI">new API</a>

- `R.APPENDINTARRAY` (append integers to a roaring bitmap)
- `R.RANGEINTARRAY` (get an integer array from a roaring bitmap with `start` and `end`, so can implements paging)
- `R.SETRANGE` (set or append integer range to a roaring bitmap)
- `R.SETFULL` (fill up a roaring bitmap in integer)
- `R.STAT` (get statistical information of a roaring bitmap)
- `R.OPTIMIZE` (optimize a roaring bitmap)

Missing commands:

- `R.BITFIELD` (same as [BITFIELD](https://redis.io/commands/bitfield))

## <a name="quickstart">quick start</a>
```
$ git clone https://github.com/aviggiano/redis-roaring.git
$ cd redis-roaring/
$ configure.sh
$ mkdir build && cd build
$ cmake ..
$ make
$ cd .. && mkdir dist
$ cp build/libredis-roaring.dylib dist
$ cp deps/redis/redis.conf dist
$ cp deps/redis/src/{redis-benchmark,redis-check-aof,redis-check-rdb,redis-cli,redis-sentinel,redis-server} dist
$ echo "loadmodule `pwd`/dist/libredis-roaring.dylib" >> dist/redis.conf
$ cd dist 
$ ./redis-server ./redis.conf  
```
then you can open another terminal and use `./redis-cli` to connect to the redis server


## Performance

Tested using CRoaring's `census1881` dataset on the travis build [337762446](https://travis-ci.org/aviggiano/redis-roaring/builds/337762446):

|           OP | TIME/OP (us) | ST.DEV. (us) |
| ------------ | ------------ | ------------ |
|     R.SETBIT |        28.21 |        27.82 |
|       SETBIT |        27.02 |        20.99 |
|     R.GETBIT |        26.57 |        11.42 |
|       GETBIT |        25.93 |         9.84 |
|   R.BITCOUNT |        26.96 |         0.03 |
|     BITCOUNT |       185.15 |         1.05 |
|     R.BITPOS |        27.16 |         0.07 |
|       BITPOS |        61.31 |         2.47 |
|  R.BITOP NOT |       101.53 |         1.75 |
|    BITOP NOT |       294.59 |         4.60 |
|  R.BITOP AND |        39.76 |         0.37 |
|    BITOP AND |       723.38 |        13.28 |
|   R.BITOP OR |        49.18 |         1.15 |
|     BITOP OR |       401.55 |         6.99 |
|  R.BITOP XOR |        57.31 |         1.45 |
|    BITOP XOR |       525.76 |         9.14 |
