reroaring [![Build Status](https://travis-ci.org/aviggiano/reroaring.svg?branch=master)](https://travis-ci.org/aviggiano/reroaring)
===========
Roaring Bitmaps for Redis

## Intro

This project uses the [CRoaring](https://github.com/RoaringBitmap/CRoaring) library to implement roaring bitmap commands for Redis.

Pull requests are welcome.

## Dependencies

Initialize and update the CRoaring git submodule and follow the build instructions on their repository.

Alternatively you can run the `./configure` script and it will compile the source code as a static library which will be linked to **reroaring** through `cmake`.

## Build

This project can be built with `cmake`:

```bash
mkdir -p build
cd build
cmake ..
make
```

## Tests

```bash
mkdir -p build
cd build
cmake -DCOMPILE_MODE:STRING="test" ..
make
./test_reroaring
```

## API

Currently only `R.SETBIT` and `R.GETBIT` operations are supported as a proof-of-concept. They are the same as redis' [SETBIT](https://redis.io/commands/setbit) and [GETBIT](https://redis.io/commands/getbit) commands on the underlying `roaring_bitmap_t` data structure.

More commands coming soon.
