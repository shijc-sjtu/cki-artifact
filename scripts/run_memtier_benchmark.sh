#!/bin/bash -e

APP=${1:-redis}
CLIENTS=${2:-1}
SERVER=${3:-192.168.12.240}

# Flush the ARP table
sudo ip -s -s neigh flush all

if [[ $APP == "redis" ]]; then
    PORT=6379
    PROTOCOL="redis"
    redis-cli -h $SERVER flushall # Clear all K/V
else # $APP == "memcached"
    PORT=11211
    PROTOCOL="memcache_text"
    nc -N $SERVER $PORT <<< "flush_all" # Clear all K/V
fi

# Run memtier_benchmark
docker run -it --rm --net=host --name memtier_benchmark redislabs/memtier_benchmark:latest \
        -P $PROTOCOL -p $PORT -s $SERVER --hide-histogram --pipeline=11 -c $CLIENTS -t 1 --run-count=1 -d 500 \
        --key-maximum=500000 --key-pattern=G:G --key-stddev=34167 \
        --ratio=1:1 --distinct-client-seed --randomize --test-time=30

echo "Benchmark done"
