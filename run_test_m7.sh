#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7794

pkill -9 -f game_server 2>/dev/null
pkill -9 -f player_simulator 2>/dev/null
sleep 0.5

cd "$REPO"

# 1. Compile Latest
cmake --build build --parallel 4
if [ $? -ne 0 ]; then echo "BUILD FAILED"; exit 1; fi

# 2. Launch Server with 512 threads to comfortably handle high-concurrency stress tests
"$REPO/build/server/game_server" --port $PORT --threads 512 > /tmp/game_server_m7.log 2>&1 &
SERVER_PID=$!
sleep 0.5

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"; cat /tmp/game_server_m7.log; exit 1
fi

echo "========================================================="
echo " Milestone 7: Automated Load Testing Suite Execution     "
echo "========================================================="

# 3. Run the full load testing suite (Baseline 50, Moderate 100, High 250, Stress 500)
python3 "$REPO/python/run_suite.py" --port $PORT --simulator "$REPO/build/simulator/player_simulator"
SUITE_EXIT=$?

# 4. Run the breaking-point saturation test
echo ""
echo "========================================================="
echo " Milestone 7: Breaking Point & Saturation Test           "
echo "========================================================="
python3 "$REPO/python/find_breaking_point.py" --port $PORT --simulator "$REPO/build/simulator/player_simulator"
BREAK_EXIT=$?

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

[ $SUITE_EXIT -eq 0 ] && [ $BREAK_EXIT -eq 0 ]
exit $?
