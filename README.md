redis-roaring [![Build Status](https://travis-ci.org/aviggiano/redis-roaring.svg?branch=master)](https://travis-ci.org/aviggiano/redis-roaring)
===========
Roaring Bitmaps for Redis

## Intro

This project uses the [CRoaring](https://github.com/RoaringBitmap/CRoaring) library to implement roaring bitmap commands for Redis.
These commands have the same performance as redis' native bitmaps for *O(1)* operations and are [up to 8x faster](#performance) for *O(N)*
calls, while consuming less memory than their uncompressed counterparts (benchmark pending).

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

Tested using CRoaring's `census1881` dataset on the travis build [256602847](https://travis-ci.org/aviggiano/redis-roaring/builds/256602847):

|           OP | TIME/OP (us) |
| ------------ | ------------ |
|     R.SETBIT |        34.26 |
|       SETBIT |        32.51 |
|     R.GETBIT |        32.17 |
|       GETBIT |        31.03 |
|   R.BITCOUNT |        32.37 |
|     BITCOUNT |       188.06 |
|     R.BITPOS |        32.44 |
|       BITPOS |        54.17 |
|  R.BITOP NOT |       104.83 |
|    BITOP NOT |       278.32 |
|  R.BITOP AND |        44.38 |
|    BITOP AND |       443.98 |
|   R.BITOP OR |        55.27 |
|     BITOP OR |       369.59 |
|  R.BITOP XOR |        54.81 |
|    BITOP XOR |       402.89 |
