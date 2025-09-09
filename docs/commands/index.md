# Roaring Bitmap Commands

This document provides an index of all available Roaring bitmap commands with links to their detailed documentation.

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
