redis-roaring [![Build Status](https://travis-ci.org/aviggiano/redis-roaring.svg?branch=master)](https://travis-ci.org/aviggiano/redis-roaring)
===========
Roaring Bitmaps for Redis

## Intro

This project uses the [CRoaring](https://github.com/RoaringBitmap/CRoaring) library to implement roaring bitmap commands for Redis.
These commands can have the same performance as redis' native bitmaps for *O(1)* operations and be [up to 8x faster](#performance) for *O(N)*
calls, according to microbenchmarks, while consuming less memory than their uncompressed counterparts (benchmark pending).

Pull requests are welcome.

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

Missing commands:

- `R.BITFIELD` (same as [BITFIELD](https://redis.io/commands/bitfield))

## Performance

Tested using CRoaring's `census1881` dataset on the travis build [337720166](https://travis-ci.org/aviggiano/redis-roaring/builds/337720166):

|           OP | TIME/OP (us) | ST.DEV. (us) |
| ------------ | ------------ | ------------ |
|     R.SETBIT |        35.85 |        18.59 |
|       SETBIT |        34.03 |        17.36 |
|     R.GETBIT |        33.88 |         8.36 |
|       GETBIT |        32.84 |         7.82 |
|   R.BITCOUNT |        32.38 |         0.06 |
|     BITCOUNT |       179.11 |         0.93 |
|     R.BITPOS |        34.23 |         0.05 |
|       BITPOS |        55.10 |         0.63 |
|  R.BITOP NOT |       107.76 |         1.42 |
|    BITOP NOT |       288.39 |         4.26 |
|  R.BITOP AND |        48.29 |         0.32 |
|    BITOP AND |       377.56 |         6.53 |
|   R.BITOP OR |        58.08 |         1.02 |
|     BITOP OR |       376.41 |         6.45 |
|  R.BITOP XOR |        61.50 |         1.16 |
|    BITOP XOR |       383.20 |         6.48 |
