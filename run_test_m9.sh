#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
REDIS_BIN="$HOME/redis-stable/src/redis-server"
REDIS_CLI="$HOME/redis-stable/src/redis-cli"
REDIS_PORT=6379

PORT_NO_REDIS=7796
PORT_WITH_REDIS=7797

pkill -9 -f game_server 2>/dev/null
pkill -9 -f player_simulator 2>/dev/null
pkill -9 -f redis-server 2>/dev/null
sleep 0.5

# 1. Start Official Native Redis Server v8.10.1
echo "=== Starting Official Native Redis Server v8.10.1 ==="
"$REDIS_BIN" --port $REDIS_PORT --daemonize yes --logfile /tmp/redis_official.log
sleep 1.0

# Verify with official redis-cli
PONG_RESP=$("$REDIS_CLI" -p $REDIS_PORT ping)
if [ "$PONG_RESP" != "PONG" ]; then
    echo "ERROR: Official Redis failed to respond to PING (got '$PONG_RESP')"
    cat /tmp/redis_official.log
    exit 1
fi
echo "=== Official Redis Server Verified: $PONG_RESP ==="
"$REDIS_CLI" -p $REDIS_PORT info server | grep -E 'redis_version|os|process_id'

# 2. Build Server & Simulator
cd "$REPO"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
if [ $? -ne 0 ]; then echo "BUILD FAILED"; exit 1; fi
echo "=== Build OK ==="

echo ""
echo "================================================================="
echo " Experiment 1: WITHOUT REDIS (In-Memory Direct Fallback)"
echo "================================================================="
"$REPO/build/server/game_server" --port $PORT_NO_REDIS --threads 128 > /tmp/srv_no_redis.log 2>&1 &
PID1=$!
sleep 0.5

python3 "$REPO/tests/test_redis_benchmark.py" --port $PORT_NO_REDIS --players 100 --duration 5 --simulator "$REPO/build/simulator/player_simulator" --out-json /tmp/m9_no_redis.json

kill $PID1 2>/dev/null
wait $PID1 2>/dev/null
sleep 1

echo ""
echo "================================================================="
echo " Experiment 2: WITH REDIS (Official Redis v8.10.1 Caching Layer)"
echo "================================================================="
"$REDIS_CLI" -p $REDIS_PORT flushall > /dev/null

"$REPO/build/server/game_server" --port $PORT_WITH_REDIS --threads 128 --enable-redis --redis-host 127.0.0.1 --redis-port $REDIS_PORT > /tmp/srv_with_redis.log 2>&1 &
PID2=$!
sleep 0.5

python3 "$REPO/tests/test_redis_benchmark.py" --port $PORT_WITH_REDIS --players 100 --duration 5 --simulator "$REPO/build/simulator/player_simulator" --out-json /tmp/m9_with_redis.json

echo ""
echo "=== Inspecting Official Redis Memory & Keys via redis-cli ==="
"$REDIS_CLI" -p $REDIS_PORT dbsize
"$REDIS_CLI" -p $REDIS_PORT keys 'session:player:*' | head -5
"$REDIS_CLI" -p $REDIS_PORT info memory | grep -E 'used_memory_human|used_memory_peak_human'

kill $PID2 2>/dev/null
wait $PID2 2>/dev/null

# Clean shutdown
"$REDIS_CLI" -p $REDIS_PORT shutdown nosave 2>/dev/null
echo ""
echo "=== Official Redis Benchmark Experiment Complete ==="
