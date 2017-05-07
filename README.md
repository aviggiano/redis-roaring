reroaring
=========
Roaring Bitmaps for Redis

## Intro

This project uses the [CRoaring](https://github.com/RoaringBitmap/CRoaring) library to implement roaring bitmap commands for Redis.

Pull requests are welcome.

## Dependencies

Initialize and update the CRoaring git submodule and follow the build instructions on their repository.

Otherwise, you can simply run the `./configure` script and it will compile the source code as a static library which will be linked to **reroaring** through `cmake`.

## Build

This project can be built with `cmake`:

```
cd deps/CRoaring/
mkdir -p build
cd build
cmake ..
make
```