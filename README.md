redis-roaring [![Build Status](https://travis-ci.org/aviggiano/redis-roaring.svg?branch=master)](https://travis-ci.org/aviggiano/redis-roaring)
===========
Roaring Bitmaps for Redis

## Intro

This project uses the [CRoaring](https://github.com/RoaringBitmap/CRoaring) library to implement roaring bitmap commands for Redis.

Pull requests are welcome.

## Dependencies

- CRoaring (redis module)
- cmake (build)
- redis (tests)

## Build

Run the `configure.sh` script and it will compile the source code as a static library which will be linked to **redis-roaring**. Alternatively initialize and update the CRoaring git submodule and follow the build instructions on their repository.

Then build the project with cmake and it will generate the Redis Module shared library.

```bash
mkdir -p build
cd build
cmake ..
make
```

## Tests

Run the `test.sh` script for unit tests and integration tests.

## API

Currently the following operations are supported

- `R.SETBIT` (same as [SETBIT](https://redis.io/commands/setbit))
- `R.GETBIT` (same as [GETBIT](https://redis.io/commands/getbit))
- `R.BITOP` (same as [BITOP](https://redis.io/commands/bitop))
- `R.BITCOUNT` (same as [BITCOUNT](https://redis.io/commands/bitcount) without `start` and `end` parameters)
- `R.BITPOS` (same as [BITPOS](https://redis.io/commands/bitpos) without `start` and `end` parameters)

More commands coming soon:

- `R.BITFIELD` (same as [BITFIELD](https://redis.io/commands/bitfield))
- `R.SETBITMAP` (create a roaring bitmap from an uncompressed bitmap)
- `R.GETBITMAP` (get an uncompressed bitmap from an roaring bitmap)
- `R.SETARRAY` (create a roaring bitmap from an integer array)
- `R.GETARRAY` (get an integer array from an roaring bitmap)
