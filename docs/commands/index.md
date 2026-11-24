# Module commands

This reference covers the standalone redis-roaring module.
For native commands in Redis 8.12 and later, see the [Redis bitmap documentation](https://redis.io/docs/latest/develop/data-types/strings/bitmaps/#roaring-compressed-bitmaps).
See the [migration guide](../guide/migrate-to-redis.md) for command changes when moving an existing application.

## Key Differences Between 32-bit and 64-bit Commands

| Aspect           | R.\* (32-bit)               | R64.\* (64-bit)                   |
| ---------------- | --------------------------- | --------------------------------- |
| **Value Range**  | 0 to 2^32-1 (≈4.3 billion)  | 0 to 2^64-1 (≈18 quintillion)     |
| **Memory Usage** | Lower for small datasets    | Higher but supports larger ranges |
| **Performance**  | Optimized for 32-bit values | Optimized for 64-bit values       |

## Documentation Conventions

- Commands use `R.` (32-bit) and `R64.` (64-bit) prefixes to distinguish them from standard Redis commands
- 32-bit commands are suitable for most general use cases
- 64-bit commands are designed for applications requiring large value ranges
- Both command sets provide identical APIs where available, differing only in supported value ranges
