#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7798

pkill -9 -f game_server 2>/dev/null
pkill -9 -f player_simulator 2>/dev/null
sleep 0.5

# 1. Verify MySQL Service is running
mysql -u game_user -pgame_pass -h 127.0.0.1 -D game_server -e "SELECT 'MySQL Connection OK' AS status;"
if [ $? -ne 0 ]; then echo "ERROR: MySQL is not reachable on port 3306"; exit 1; fi
echo "=== MySQL 8.0 Ready on Port 3306 ==="

# 2. Build Server & Simulator with MySQL connector
cd "$REPO"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
if [ $? -ne 0 ]; then echo "BUILD FAILED"; exit 1; fi
echo "=== Build OK (Linked with libmysqlclient) ==="

# 3. Launch Server connecting to MySQL 8.0
"$REPO/build/server/game_server" --port $PORT --threads 8 --db-host 127.0.0.1 --db-port 3306 --db-user game_user --db-pass game_pass --db-name game_server > /tmp/srv_m10_mysql.log 2>&1 &
SERVER_PID=$!
sleep 0.8

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"; cat /tmp/srv_m10_mysql.log; exit 1
fi

# 4. Run MySQL Test Suite
python3 "$REPO/tests/test_mysql_persistence.py" --port $PORT
TEST_EXIT=$?

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo ""
echo "=== Direct MySQL Query Verification via mysql CLI ==="
mysql -u game_user -pgame_pass -h 127.0.0.1 -D game_server -e "
SELECT player_id, username, score, high_score, kills, created_at, updated_at FROM players WHERE player_id IN (999, 998, 997);
SELECT event_id, match_id, player_id, event_type, event_data, created_at FROM game_events ORDER BY event_id DESC LIMIT 5;
"

exit $TEST_EXIT
