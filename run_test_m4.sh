#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7791

pkill -9 -f game_server 2>/dev/null
sleep 0.5

cd "$REPO"
cmake --build build --parallel 4 2>&1
BUILD_EXIT=$?
if [ $BUILD_EXIT -ne 0 ]; then echo "BUILD FAILED"; exit 1; fi
echo "=== Build OK ==="

"$REPO/build/server/game_server" --port $PORT > /tmp/game_server_m4.log 2>&1 &
SERVER_PID=$!
sleep 0.5

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"; cat /tmp/game_server_m4.log; exit 1
fi

python3 "$REPO/test_milestone4.py" --port $PORT
TEST_EXIT=$?

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Server log ==="
cat /tmp/game_server_m4.log

exit $TEST_EXIT
