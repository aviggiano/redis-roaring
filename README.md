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

- `R.SETBIT`
- `R.GETBIT`
- `R.BITOP`

They have the same the same semantycs as redis' [SETBIT](https://redis.io/commands/setbit),
[GETBIT](https://redis.io/commands/getbit) and [BITOP](https://redis.io/commands/bitop).

More commands coming soon.
