#!/bin/bash
# Run Milestone 2 tests — called from WSL
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7779

# Kill any leftover server processes
pkill -f game_server 2>/dev/null
sleep 0.3

cd "$REPO"

# Start server in background, capture logs
"$REPO/build/server/game_server" --port $PORT --debug > /tmp/game_server_m2.log 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"
sleep 0.5

# Check it started
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"
    cat /tmp/game_server_m2.log
    exit 1
fi

# Run Python tests
python3 "$REPO/test_milestone2.py" --port $PORT
TEST_EXIT=$?

# Shutdown
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Server log ==="
cat /tmp/game_server_m2.log

exit $TEST_EXIT
