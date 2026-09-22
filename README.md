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

This module is intended to support **Redis 6.2 and 7.0** (the versions we test in CI).

- **Redis 7.4+**: Full feature support including custom ACL categories, command ACL categories, and command introspection
- **Redis 7.2 - 7.3**: Command introspection and command ACL categories supported
- **Redis 7.0 - 7.1**: Command introspection (COMMAND INFO, COMMAND DOCS) supported
- **Redis 6.2**: Core roaring bitmap functionality supported

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

## Migrating to Redis native bitmaps

After [redis/redis#15296](https://github.com/redis/redis/issues/15296),
core Redis supports native Roaring-backed bitmaps. We recommend migrating
redis-roaring keys to core Redis for long-term upstream support, standard Redis
persistence and replication behavior, and simpler operational tooling.

The standalone `tools/redis-bitmap-migrate.py` utility migrates `R.*` and
`R64.*` redis-roaring keys into Redis native bitmap values through public Redis
and redis-roaring commands. It streams integer ranges from the source, writes
native bitmap values on the target, validates temporary keys, and records a
durable JSON manifest. It does not depend on Redis core internals or
redis-roaring private payload layouts.

By default the tool performs a dry run:

```bash
python3 tools/redis-bitmap-migrate.py \
  --source-host 127.0.0.1 --source-port 6379 \
  --target-host 127.0.0.1 --target-port 6380 \
  --manifest redis-bitmap-migrate-manifest.json
```

To write destination keys, run a final pass while source writes are frozen and
acknowledge that with `--apply --assume-frozen`.

```bash
python3 tools/redis-bitmap-migrate.py \
  --source-host 127.0.0.1 --source-port 6379 \
  --target-host 127.0.0.1 --target-port 6380 \
  --manifest redis-bitmap-migrate-manifest.json \
  --apply --assume-frozen
```

Run the migrator contract tests with:

```bash
python3 -m py_compile tools/redis-bitmap-migrate.py tests/integration/redis-bitmap-migrate.py
python3 tests/integration/redis-bitmap-migrate.py
```

The default test run uses fake RESP servers to verify the public-command
contract. Real Redis-to-native integration tests are enabled when
`REDIS_NATIVE_SERVER`, `REDIS_ROARING_SERVER`, and `REDIS_ROARING_MODULE` point
to built binaries.

## Fuzzing

redis-roaring includes fuzz testing using libFuzzer with AddressSanitizer and UndefinedBehaviorSanitizer. Five fuzz targets cover 32-bit operations, 64-bit operations, complex bitwise operations, data parsing, and BITOP key-discovery metadata. Fuzzing runs automatically on every push to master.

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
|         R.SETBIT |            57.31 |            16.63 |
|       R64.SETBIT |            57.99 |            17.83 |
|           SETBIT |            56.34 |            17.87 |
|         R.GETBIT |            57.22 |            12.47 |
|       R64.GETBIT |            56.51 |            15.34 |
|           GETBIT |            55.20 |            11.86 |
|       R.BITCOUNT |            67.42 |             0.12 |
|     R64.BITCOUNT |            66.29 |             0.05 |
|         BITCOUNT |            81.93 |             0.14 |
|         R.BITPOS |            66.63 |             0.05 |
|       R64.BITPOS |            67.49 |             0.19 |
|           BITPOS |            73.79 |             0.25 |
|      R.BITOP NOT |           117.64 |             1.30 |
|    R64.BITOP NOT |           123.35 |             1.28 |
|        BITOP NOT |           245.22 |             1.33 |
|      R.BITOP AND |            75.30 |             0.28 |
|    R64.BITOP AND |            75.88 |             0.21 |
|        BITOP AND |           293.38 |             3.84 |
|       R.BITOP OR |            71.02 |             1.20 |
|     R64.BITOP OR |            70.21 |             0.72 |
|         BITOP OR |           438.50 |             7.06 |
|      R.BITOP XOR |            84.03 |             1.17 |
|    R64.BITOP XOR |            90.56 |             0.77 |
|        BITOP XOR |           410.52 |             6.16 |
|    R.BITOP ANDOR |            74.87 |             0.16 |
|  R64.BITOP ANDOR |            79.27 |             0.23 |
|      BITOP ANDOR |           425.19 |             6.46 |
|      R.BITOP ONE |            86.96 |             0.72 |
|    R64.BITOP ONE |            95.55 |             0.91 |
|        BITOP ONE |           450.55 |             7.02 |
|            R.MIN |            68.83 |             0.20 |
|          R64.MIN |            66.94 |             0.04 |
|              MIN |            67.34 |             0.08 |
|            R.MAX |            67.50 |             0.05 |
|          R64.MAX |            68.92 |             0.10 |
|              MAX |            58.44 |             0.10 |
<!-- END_PERFORMANCE -->
