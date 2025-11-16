redis-roaring ![CI/CD](https://github.com/aviggiano/redis-roaring/actions/workflows/ci.yml/badge.svg) [![Static Badge](https://img.shields.io/badge/documentation-passing-blue)](https://redisroaring.com)
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
|         R.SETBIT |            19.44 |             4.00 |
|       R64.SETBIT |            19.35 |             5.75 |
|           SETBIT |            18.90 |             4.99 |
|         R.GETBIT |            18.74 |             1.28 |
|       R64.GETBIT |            18.80 |             1.25 |
|           GETBIT |            18.40 |             2.31 |
|       R.BITCOUNT |            14.96 |             0.04 |
|     R64.BITCOUNT |            14.76 |             0.03 |
|         BITCOUNT |            38.18 |             0.17 |
|         R.BITPOS |            19.69 |             0.05 |
|       R64.BITPOS |            20.17 |             0.08 |
|           BITPOS |            27.92 |             0.27 |
|      R.BITOP NOT |            38.39 |             0.52 |
|    R64.BITOP NOT |            36.97 |             0.54 |
|        BITOP NOT |            62.43 |             0.32 |
|      R.BITOP AND |            22.95 |             0.10 |
|    R64.BITOP AND |            28.89 |             0.20 |
|        BITOP AND |           163.28 |             2.75 |
|       R.BITOP OR |            27.29 |             0.64 |
|     R64.BITOP OR |            25.09 |             0.16 |
|         BITOP OR |           210.42 |             3.75 |
|      R.BITOP XOR |            23.68 |             0.15 |
|    R64.BITOP XOR |            24.83 |             0.17 |
|        BITOP XOR |           185.91 |             3.24 |
|    R.BITOP ANDOR |            22.97 |             0.09 |
|  R64.BITOP ANDOR |            23.45 |             0.11 |
|      BITOP ANDOR |           211.66 |             3.74 |
|      R.BITOP ONE |            23.86 |             0.15 |
|    R64.BITOP ONE |            26.36 |             0.21 |
|        BITOP ONE |           209.89 |             3.70 |
|            R.MIN |            19.63 |             0.03 |
|          R64.MIN |            20.33 |             0.07 |
|              MIN |            20.36 |             0.06 |
|            R.MAX |            19.85 |             0.04 |
|          R64.MAX |            19.49 |             0.03 |
|              MAX |            19.94 |             0.06 |
<!-- END_PERFORMANCE -->
