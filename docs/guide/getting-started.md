# Module installation

These instructions are for the standalone redis-roaring module in maintenance mode.
For a new deployment, [use native Roaring bitmaps](https://redis.io/docs/latest/develop/data-types/strings/bitmaps/#roaring-compressed-bitmaps) on Redis 8.12 or later.
To move an existing deployment, [follow the migration guide](./migrate-to-redis.md).

## Manual Compilation

```bash
git clone https://github.com/aviggiano/redis-roaring.git
cd redis-roaring/
./configure.sh
cd dist
./redis-server ./redis.conf
```

In a second terminal, run `./redis-cli` from the `dist` directory to connect to Redis.

## Docker

Run the standalone module in a container with Docker.

```bash
docker run -p 6379:6379 aviggiano/redis-roaring:latest
```
