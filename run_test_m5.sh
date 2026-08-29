#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7792

pkill -9 -f game_server 2>/dev/null
sleep 0.5

cd "$REPO"
cmake --build build --parallel 4 2>&1
if [ $? -ne 0 ]; then echo "BUILD FAILED"; exit 1; fi
echo "=== Build OK ==="

# Run with 8 threads explicitly so result is reproducible
"$REPO/build/server/game_server" --port $PORT --threads 8 > /tmp/game_server_m5.log 2>&1 &
SERVER_PID=$!
sleep 0.5

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"; cat /tmp/game_server_m5.log; exit 1
fi

# Test 1: 50 concurrent players
echo "--- Test: 50 concurrent players ---"
python3 "$REPO/test_milestone5.py" --port $PORT --players 50
T1=$?

# Test 2: 100 concurrent players
echo ""
echo "--- Test: 100 concurrent players ---"
python3 "$REPO/test_milestone5.py" --port $PORT --players 100
T2=$?

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Server log (last 15 lines) ==="
tail -15 /tmp/game_server_m5.log

# Exit non-zero if either test failed
[ $T1 -eq 0 ] && [ $T2 -eq 0 ]
exit $?
