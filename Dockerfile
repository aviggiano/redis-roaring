FROM redis

ENV ROARING_REDIS_GIT_URL https://github.com/aviggiano/redis-roaring.git
RUN set -ex; \
    \
    buildDeps=' \
        ca-certificates
        cmake \
        gcc \
        git \
        g++ \
        make \
    '; \
    apt-get update; \
    apt-get install -y $buildDeps --no-install-recommends; \
    rm -rf /var/lib/apt/lists/*; \
    git clone "$ROARING_REDIS_GIT_URL"; \
    cd redis-roaring; \
    bash configure.sh; \
    mkdir -p build; \
    cd build; \
    cmake ..; \
    make -j; \
    cp libredis-roaring.so /usr/local/lib/; \
    cd ../../; \
    rm -rf redis-roaring; \
    cd redis-roaring; \
    bash configure.sh; \
    \
    apt-get purge -y --auto-remove $buildDeps
ADD http://download.redis.io/redis-stable/redis.conf /usr/local/etc/redis/redis.conf
RUN sed -i '1i loadmodule /usr/local/lib/libredis-roaring.so' /usr/local/etc/redis/redis.conf; \
    chmod 644 /usr/local/etc/redis/redis.conf; \
    sed -i 's/^bind 127.0.0.1/#bind 127.0.0.1/g' /usr/local/etc/redis/redis.conf; \
    sed -i 's/^protected-mode yes/protected-mode no/g' /usr/local/etc/redis/redis.conf
CMD [ "redis-server", "/usr/local/etc/redis/redis.conf" ]
