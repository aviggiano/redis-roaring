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

Tested using CRoaring's `census1881` dataset on the travis build [271260560](https://travis-ci.org/aviggiano/redis-roaring/builds/271260560):

|           OP | TIME/OP (us) | ST.DEV. (us) |
| ------------ | ------------ | ------------ |
|     R.SETBIT |        24.64 |        22.75 |
|       SETBIT |        24.04 |        20.97 |
|     R.GETBIT |        23.46 |        12.27 |
|       GETBIT |        23.08 |        11.61 |
|   R.BITCOUNT |        23.82 |         0.02 |
|     BITCOUNT |       175.70 |         1.00 |
|     R.BITPOS |        20.95 |         0.05 |
|       BITPOS |        48.10 |         0.69 |
|  R.BITOP NOT |        90.28 |         1.41 |
|    BITOP NOT |       292.32 |         4.78 |
|  R.BITOP AND |        32.57 |         0.32 |
|    BITOP AND |       699.05 |        13.03 |
|   R.BITOP OR |        44.63 |         1.03 |
|     BITOP OR |       389.68 |         6.97 |
|  R.BITOP XOR |        45.55 |         1.06 |
|    BITOP XOR |       501.65 |         8.91 |
