redis-roaring [![Coverage Status](https://coveralls.io/repos/github/aviggiano/redis-roaring/badge.svg?branch=master)](https://coveralls.io/github/aviggiano/redis-roaring?branch=master) [![CI/CD](https://github.com/aviggiano/redis-roaring/actions/workflows/ci.yml/badge.svg)](https://github.com/aviggiano/redis-roaring/actions/workflows/ci.yml) [![Static Badge](https://img.shields.io/badge/documentation-passing-blue)](https://redisroaring.com)
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

## Redis Version Compatibility

This module is compatible with **Redis 6.0 and later**.

- **Redis 7.4+**: Full feature support including custom ACL categories, command ACL categories, and command introspection
- **Redis 7.2 - 7.3**: Command introspection and command ACL categories supported
- **Redis 7.0 - 7.1**: Command introspection (COMMAND INFO, COMMAND DOCS) supported
- **Redis 6.0 - 6.2**: Core roaring bitmap functionality supported

The module automatically detects the Redis version at runtime and adjusts its behavior accordingly:
- Command metadata (via `RedisModule_SetCommandInfo`) is only registered on Redis 7.0+
- Command ACL categories (via `RedisModule_SetCommandACLCategories`) are only set on Redis 7.2+
- Custom ACL categories (via `RedisModule_AddACLCategory`) are only registered on Redis 7.4+

All core roaring bitmap commands work on Redis 6.0+.

## Getting started

```
$ git clone https://github.com/aviggiano/redis-roaring.git
$ cd redis-roaring/
$ configure.sh
$ cd dist 
$ ./redis-server ./redis.conf  
```
then you can open another terminal and use `./redis-cli` to connect to the redis server

## Docker

It is also possible to run this project as a docker container.

```bash
docker run -p 6379:6379 aviggiano/redis-roaring:latest
```

## Tests

Run the `test.sh` script for unit tests, integration tests and performance tests.
The performance tests can take a while, since they run on a real dataset of integer values.

## Fuzzing

redis-roaring includes fuzz testing using libFuzzer with AddressSanitizer and UndefinedBehaviorSanitizer. Four fuzz targets test 32-bit operations, 64-bit operations, complex bitwise operations, and data parsing. Fuzzing runs automatically on every push to master.

Build and run fuzzers locally with `./scripts/build_fuzzers.sh` (requires Clang). Run individual fuzzers with `./scripts/run_fuzzer.sh <fuzzer_name> <duration>` or all fuzzers with `./scripts/run_all_fuzzers.sh`.

See [docs/fuzzing.md](docs/fuzzing.md) for complete documentation.

## API

**📖 Complete Documentation**: For detailed command documentation including syntax, parameters, examples, and usage notes, see [docs/commands/index.md](docs/commands/index.md)

The following operations are supported

- `R.SETBIT` (same as [SETBIT](https://redis.io/commands/setbit))
- `R.GETBIT` (same as [GETBIT](https://redis.io/commands/getbit))
- `R.BITOP` (same as [BITOP](https://redis.io/commands/bitop))
- `R.BITCOUNT` (same as [BITCOUNT](https://redis.io/commands/bitcount) without `start` and `end` parameters)
- `R.BITPOS` (same as [BITPOS](https://redis.io/commands/bitpos) without `start` and `end` parameters)
- `R.SETINTARRAY` (create a roaring bitmap from an integer array)
- `R.GETINTARRAY` (get an integer array from a roaring bitmap)
- `R.SETBITARRAY` (create a roaring bitmap from a bit array string)
- `R.GETBITARRAY` (get a bit array string from a roaring bitmap)

Additional commands

- `R.APPENDINTARRAY` (append integers to a roaring bitmap)
- `R.RANGEINTARRAY` (get an integer array from a roaring bitmap with `start` and `end`, so can implements paging)
- `R.SETRANGE` (set or append integer range to a roaring bitmap)
- `R.SETFULL` (fill up a roaring bitmap in integer)
- `R.STAT` (get statistical information of a roaring bitmap)
- `R.OPTIMIZE` (optimize a roaring bitmap)
- `R.MIN` (get minimal integer from a roaring bitmap, if key is not exists or bitmap is empty, return -1)
- `R.MAX` (get maximal integer from a roaring bitmap, if key is not exists or bitmap is empty, return -1)
- `R.DIFF` (get difference between two bitmaps)

64-bit bitmap commands (for handling values beyond 32-bit range)

- `R64.SETBIT` (64-bit version of SETBIT)
- `R64.GETBIT` (64-bit version of GETBIT)
- `R64.SETINTARRAY` (create a 64-bit roaring bitmap from an integer array)
- `R64.GETINTARRAY` (get an integer array from a 64-bit roaring bitmap)
- `R64.RANGEINTARRAY` (get an integer array from a 64-bit roaring bitmap with `start` and `end`)
- `R64.APPENDINTARRAY` (append integers to a 64-bit roaring bitmap)
- `R64.DIFF` (get difference between two 64-bit bitmaps)
- `R64.SETFULL` (fill up a 64-bit roaring bitmap)

Missing commands:

- `R.BITFIELD` (same as [BITFIELD](https://redis.io/commands/bitfield))

## API Example
```
$ redis-cli
# create a roaring bitmap with numbers from 1 to 99
127.0.0.1:6379> R.SETRANGE test 1 100

# get all the numbers as an integer array
127.0.0.1:6379> R.GETINTARRAY test

# fill up the roaring bitmap 
# because you need 2^32*4 bytes memory and a very long time
127.0.0.1:6379> R.SETFULL full

# use `R.RANGEINTARRAY` to get numbers from 100 to 1000 
127.0.0.1:6379> R.RANGEINTARRAY full 100 1000

# append numbers to an existing roaring bitmap
127.0.0.1:6379> R.APPENDINTARRAY test 111 222 3333 456 999999999 9999990
```

## Performance

Tested using CRoaring's `census1881` dataset. Performance tests are run automatically on every push to master branch.

<!-- BEGIN_PERFORMANCE -->
|               OP |     TIME/OP (us) |     ST.DEV. (us) |
| ---------------- | ---------------- | ---------------- |
|         R.SETBIT |            47.48 |            13.65 |
|       R64.SETBIT |            47.39 |            12.34 |
|           SETBIT |            45.75 |            11.81 |
|         R.GETBIT |            46.55 |             7.57 |
|       R64.GETBIT |            46.38 |             7.16 |
|           GETBIT |            45.66 |             6.77 |
|       R.BITCOUNT |            48.37 |             0.10 |
|     R64.BITCOUNT |            43.09 |             0.07 |
|         BITCOUNT |            57.34 |             0.12 |
|         R.BITPOS |            43.63 |             0.07 |
|       R64.BITPOS |            43.63 |             0.16 |
|           BITPOS |            49.02 |             0.20 |
|      R.BITOP NOT |            89.74 |             1.28 |
|    R64.BITOP NOT |            94.75 |             1.27 |
|        BITOP NOT |           207.97 |             1.19 |
|      R.BITOP AND |            49.81 |             0.22 |
|    R64.BITOP AND |            50.98 |             0.19 |
|        BITOP AND |           262.08 |             3.69 |
|       R.BITOP OR |            60.95 |             1.17 |
|     R64.BITOP OR |            61.12 |             0.68 |
|         BITOP OR |           418.35 |             6.93 |
|      R.BITOP XOR |            61.11 |             1.10 |
|    R64.BITOP XOR |            62.62 |             0.77 |
|        BITOP XOR |           377.55 |             6.07 |
|    R.BITOP ANDOR |            47.79 |             0.16 |
|  R64.BITOP ANDOR |            49.86 |             0.23 |
|      BITOP ANDOR |           398.88 |             6.34 |
|      R.BITOP ONE |            71.44 |             0.65 |
|    R64.BITOP ONE |            79.51 |             0.83 |
|        BITOP ONE |           426.52 |             7.02 |
|            R.MIN |            44.65 |             0.16 |
|          R64.MIN |            43.60 |             0.05 |
|              MIN |            42.74 |             0.04 |
|            R.MAX |            43.05 |             0.04 |
|          R64.MAX |            42.86 |             0.05 |
|              MAX |            43.15 |             0.04 |
<!-- END_PERFORMANCE -->
