#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
GAME_PORT=7800
METRICS_PORT=9100

pkill -9 -f game_server 2>/dev/null
pkill -9 -f player_simulator 2>/dev/null
sleep 0.5

# 1. Build
cd "$REPO"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
if [ $? -ne 0 ]; then echo "BUILD FAILED"; exit 1; fi
echo "=== Build OK ==="

# 2. Launch Server with HTTP Metrics Exporter on port 9100
"$REPO/build/server/game_server" --port $GAME_PORT --metrics-port $METRICS_PORT --threads 8 > /tmp/srv_m12.log 2>&1 &
SERVER_PID=$!
sleep 0.8

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"; cat /tmp/srv_m12.log; exit 1
fi

# 3. Generate some gameplay traffic
python3 "$REPO/python/run_test.py" --port $GAME_PORT --players 20 --duration 3 --interval 10

# 4. Test Prometheus HTTP Exporter
python3 "$REPO/tests/test_prometheus_exporter.py" --port $METRICS_PORT
TEST_EXIT=$?

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

exit $TEST_EXIT
