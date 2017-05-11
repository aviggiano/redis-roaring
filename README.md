reroaring [![Build Status](https://travis-ci.org/aviggiano/reroaring.svg?branch=master)](https://travis-ci.org/aviggiano/reroaring)
===========
Roaring Bitmaps for Redis

## Intro

This project uses the [CRoaring](https://github.com/RoaringBitmap/CRoaring) library to implement roaring bitmap commands for Redis.

Pull requests are welcome.

## Dependencies

Initialize and update the CRoaring git submodule and follow the build instructions on their repository.

Alternatively you can run the `configure.sh` script and it will compile the source code as a static library which will be linked to **reroaring** through `cmake`.

## Build

This project can be built with the `build.sh` script, or more specifically:

```bash
mkdir -p build
cd build
cmake ..
make
```

## Tests

Just run the `test.sh` script, or by hand:

```bash
mkdir -p build
cd build
cmake -DCOMPILE_MODE:STRING="test" ..
make
./test_reroaring
```

## API

Currently only the following operations are supported

- `R.SETBIT` (same as [SETBIT](https://redis.io/commands/setbit))
- `R.GETBIT` (same as [GETBIT](https://redis.io/commands/getbit))
- `R.BITOP` (same as [BITOP](https://redis.io/commands/bitop))
- `R.BITCOUNT` (same as [BITCOUNT](https://redis.io/commands/bitcount) without `start` and `end` parameters)
- `R.BITPOS` (same as [BITPOS](https://redis.io/commands/bitpos) without `start` and `end` parameters)

More commands coming soon.
