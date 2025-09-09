# Getting Started

## Manual Compilation

```bash
git clone https://github.com/aviggiano/redis-roaring.git
cd redis-roaring/
configure.sh
cd dist
./redis-server ./redis.conf
```

then you can open another terminal and use `./redis-cli` to connect to the redis server

## Docker

It is also possible to run this project as a docker container.

```bash
docker run -p 6379:6379 aviggiano/redis-roaring:latest
```
