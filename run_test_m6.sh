#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7793

pkill -9 -f game_server 2>/dev/null
pkill -9 -f player_simulator 2>/dev/null
sleep 0.5

cd "$REPO"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4 2>&1
if [ $? -ne 0 ]; then echo "BUILD FAILED"; exit 1; fi
echo "=== Build OK ==="

# 1. Start Server with 128 threads for true 100+ concurrent player execution
"$REPO/build/server/game_server" --port $PORT --threads 128 > /tmp/game_server_m6.log 2>&1 &
SERVER_PID=$!
sleep 0.5

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"; cat /tmp/game_server_m6.log; exit 1
fi

echo "[Test 1] Running C++ Simulator: 50 players, 5 seconds..."
"$REPO/build/simulator/player_simulator" --port $PORT --players 50 --duration 5 --interval 20 --json-out /tmp/m6_sim_50.json
SIM_EXIT_1=$?

echo "[Test 2] Running C++ Simulator: 100 players, 5 seconds..."
"$REPO/build/simulator/player_simulator" --port $PORT --players 100 --duration 5 --interval 10 --json-out /tmp/m6_sim_100.json
SIM_EXIT_2=$?

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Server log (last 10 lines) ==="
tail -10 /tmp/game_server_m6.log

echo ""
echo "=== 100-Player Benchmark JSON Result ==="
cat /tmp/m6_sim_100.json

[ $SIM_EXIT_1 -eq 0 ] && [ $SIM_EXIT_2 -eq 0 ]
exit $?
