---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "Redis Roaring"
  text: "A better compressed bitset"
  image:
    src: /logo.svg
    alt: VitePress
  actions:
    - theme: brand
      text: What is Roaring Bitmaps
      link: /guide/what-is-roaring-bitmap
    - theme: alt
      text: Quickstart
      link: /guide/getting-started
    - theme: alt
      text: Commands
      link: /commands
features:
  - icon: 🚀
    title: Performance
    details: Up to 8x faster than Redis native bitmaps for O(N) operations while maintaining O(1) performance for single-bit operations.
    link: /guide/performance

  - icon: 💾
    title: Memory Efficient
    details: Adaptive compression automatically chooses optimal storage format, often reducing memory usage by 2-10x compared to uncompressed bitmaps

  - icon: 🔧
    title: Drop-in Replacement
    details: Familiar Redis-style commands that seamlessly integrate with your existing Redis workflow and applications
    link: /commands

  - icon: ⚡
    title: Smart Compression
    details: Automatically switches between array, bitmap, and run-length encoding based on data density for optimal performance

  - icon: 🛠️
    title: Production Ready
    details: Built on the battle-tested CRoaring library with comprehensive test coverage and proven reliability in production environments

  - icon: 📊
    title: Analytics Optimized
    details: Perfect for user analytics, segmentation, real-time filtering, and any application requiring fast set operations on large datasets
---

## Why Redis Roaring?

Traditional Redis bitmaps work great for dense data but can be memory-intensive and slow for sparse datasets. **Redis Roaring** combines the familiar Redis bitmap interface with the power of Roaring Bitmaps, giving you the best of both worlds.

### The Problem with Traditional Bitmaps

```bash
# Traditional Redis bitmap for user IDs 1, 1000000
SETBIT users:active 1 1
SETBIT users:active 1000000 1
# Result: ~256KB allocated for just 2 records!
```

### The Redis Roaring Solution

```bash
# Redis Roaring bitmap for the same data
R.SETBIT users:active 1 1
R.SETBIT users:active 1000000 1
# Result: ~13 bytes allocated
```

This project leverages the industry-standard [CRoaring](https://github.com/RoaringBitmap/CRoaring) library, which powers bitmap operations

## Contributes

<TheContributors />
