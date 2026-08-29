#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7801
REDIS_BIN="$HOME/redis-stable/src/redis-server"
REDIS_CLI="$HOME/redis-stable/src/redis-cli"

pkill -9 -f game_server 2>/dev/null
pkill -9 -f player_simulator 2>/dev/null
pkill -9 -f redis-server 2>/dev/null
sleep 0.5

# 1. Start Redis
"$REDIS_BIN" --port 6379 --daemonize yes --logfile /tmp/redis_m15.log
sleep 0.8
"$REDIS_CLI" ping > /dev/null || { echo "ERROR: Redis not running"; exit 1; }

# 2. Start Game Server
cd "$REPO"
"$REPO/build/server/game_server" --port $PORT --threads 128 --enable-redis --redis-host 127.0.0.1 --redis-port 6379 > /tmp/srv_m15.log 2>&1 &
SERVER_PID=$!
sleep 0.8

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"; cat /tmp/srv_m15.log; exit 1
fi

# 3. Run Chaos & Fault Injection Suite
python3 "$REPO/python/chaos_test.py" --port $PORT --redis-cli "$REDIS_CLI"
CHAOS_EXIT=$?

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

exit $CHAOS_EXIT
