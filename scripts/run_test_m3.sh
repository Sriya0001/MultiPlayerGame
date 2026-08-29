#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7790

# Hard kill everything
pkill -9 -f game_server 2>/dev/null
sleep 1

cd "$REPO"
"$REPO/build/server/game_server" --port $PORT > /tmp/game_server_m3.log 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID  port: $PORT"
sleep 0.5

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"
    cat /tmp/game_server_m3.log
    exit 1
fi

python3 "$REPO/test_milestone3.py" --port $PORT
TEST_EXIT=$?

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Server log ==="
cat /tmp/game_server_m3.log

exit $TEST_EXIT
