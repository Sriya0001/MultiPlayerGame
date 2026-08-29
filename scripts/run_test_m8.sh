#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7795

pkill -9 -f game_server 2>/dev/null
sleep 0.5

cd "$REPO"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
if [ $? -ne 0 ]; then echo "BUILD FAILED"; exit 1; fi

"$REPO/build/server/game_server" --port $PORT --threads 8 > /tmp/game_server_m8.log 2>&1 &
SERVER_PID=$!
sleep 0.5

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"; cat /tmp/game_server_m8.log; exit 1
fi

python3 "$REPO/test_milestone8.py" --port $PORT
TEST_EXIT=$?

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

exit $TEST_EXIT
