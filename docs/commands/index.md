# Roaring Bitmap Commands

This document provides an index of all available Roaring bitmap commands with links to their detailed documentation.

## Command Availability

| 32-bit (R.\*)                               | 64-bit (R64.\*)                                 | Description                         | Notes                                                        |
| ------------------------------------------- | ----------------------------------------------- | ----------------------------------- | ------------------------------------------------------------ |
| [`R.SETBIT`](./r.setbit.md)                 | [`R64.SETBIT`](./r64.setbit.md)                 | Set bit value                       | Same as Redis [SETBIT](https://redis.io/commands/setbit)     |
| [`R.GETBIT`](./r.getbit.md)                 | [`R64.GETBIT`](./r64.getbit.md)                 | Get bit value                       | Same as Redis [GETBIT](https://redis.io/commands/getbit)     |
| [`R.BITOP`](./r.bitop.md)                   | [`R64.BITOP`](./r64.bitop.md)                   | Bitwise operations                  | Same as Redis [BITOP](https://redis.io/commands/bitop)       |
| [`R.BITCOUNT`](./r.bitcount.md)             | [`R64.BITCOUNT`](./r64.bitcount.md)             | Count set bits                      | Same as Redis [BITCOUNT](https://redis.io/commands/bitcount) |
| [`R.BITPOS`](./r.bitpos.md)                 | [`R64.BITPOS`](./r64.bitpos.md)                 | Find bit position                   | Same as Redis [BITPOS](https://redis.io/commands/bitpos)     |
| [`R.SETINTARRAY`](./r.setintarray.md)       | [`R64.SETINTARRAY`](./r64.setintarray.md)       | Create bitmap from integer array    |                                                              |
| [`R.GETINTARRAY`](./r.getintarray.md)       | [`R64.GETINTARRAY`](./r64.getintarray.md)       | Get integer array from bitmap       |                                                              |
| [`R.SETBITARRAY`](./r.setbitarray.md)       | [`R64.SETBITARRAY`](./r64.setbitarray.md)       | Create bitmap from bit array string |                                                              |
| [`R.GETBITARRAY`](./r.getbitarray.md)       | [`R64.GETBITARRAY`](./r64.setbitarray.md)       | Get bit array string from bitmap    |                                                              |
| [`R.APPENDINTARRAY`](./r.appendintarray.md) | [`R64.APPENDINTARRAY`](./r64.appendintarray.md) | Append integers to bitmap           |                                                              |
| [`R.RANGEINTARRAY`](./r.rangeintarray.md)   | [`R64.RANGEINTARRAY`](./r64.rangeintarray.md)   | Get integer array with range        |                                                              |
| [`R.SETRANGE`](./r.setrange.md)             | [`R64.SETRANGE`](./r64.setrange.md)             | Set integer range in bitmap         |                                                              |
| [`R.SETFULL`](./r.setfull.md)               | [`R64.SETFULL`](./r64.setfull.md)               | Fill bitmap with integer range      |                                                              |
| [`R.OPTIMIZE`](./r.optimize.md)             | [`R64.OPTIMIZE`](./r64.optimize.md)             | Optimize bitmap storage             |                                                              |
| [`R.MIN`](./r.min.md)                       | [`R64.MIN`](./r64.min.md)                       | Get minimum value                   |                                                              |
| [`R.MAX`](./r.max.md)                       | [`R64.MAX`](./r64.max.md)                       | Get maximum value                   |                                                              |
| [`R.DIFF`](./r.diff.md)                     | [`R64.DIFF`](./r64.diff.md)                     | Get difference between bitmaps      |                                                              |
| [`R.STAT`](./r.stat.md)                     | [`R.STAT`](./r.stat.md)                         | Get statistical information         | Supports both Bitmap32 and Bitmap64                          |

## Key Differences Between 32-bit and 64-bit Commands

| Aspect           | R.\* (32-bit)               | R64.\* (64-bit)                   |
| ---------------- | --------------------------- | --------------------------------- |
| **Value Range**  | 0 to 2^32-1 (≈4.3 billion)  | 0 to 2^64-1 (≈18 quintillion)     |
| **Memory Usage** | Lower for small datasets    | Higher but supports larger ranges |
| **Performance**  | Optimized for 32-bit values | Optimized for 64-bit values       |

## Documentation Conventions

- Commands use `R.` (32-bit) and `R64.` (64-bit) prefixes to distinguish them from standard Redis commands
- Commands that mirror Redis functionality maintain the same parameter structure and behavior
- 32-bit commands are suitable for most general use cases
- 64-bit commands are designed for applications requiring large value ranges
- Both command sets provide identical APIs where available, differing only in supported value ranges
