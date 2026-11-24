---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "Redis Roaring"
  text: "Roaring bitmaps in Redis 8.12"
  tagline: "redis-roaring is in maintenance mode. Use native Redis bitmaps for new deployments."
  image:
    src: /logo.svg
    alt: Redis Roaring logo
  actions:
    - theme: brand
      text: Use native Redis bitmaps
      link: https://redis.io/docs/latest/develop/data-types/strings/bitmaps/#roaring-compressed-bitmaps
    - theme: alt
      text: Migrate from redis-roaring
      link: /guide/migrate-to-redis
features:
  - title: Module installation
    details: Build the standalone module or run its Docker image for an existing deployment.
    link: /guide/getting-started

  - title: Module commands
    details: Look up syntax and examples for the module’s R.* and R64.* commands.
    link: /commands/

  - title: How Roaring bitmaps work
    details: Learn how Roaring bitmaps store sets of integers.
    link: /guide/what-is-roaring-bitmap
---

Maintenance of the standalone module focuses on critical fixes and migration support. [Redis 8.12.0 release notes](https://github.com/redis/redis/releases/tag/8.12.0)

## Contributors

<TheContributors />
