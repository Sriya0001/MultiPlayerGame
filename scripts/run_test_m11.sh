#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7799
REDIS_BIN="$HOME/redis-stable/src/redis-server"
REDIS_CLI="$HOME/redis-stable/src/redis-cli"

pkill -9 -f game_server 2>/dev/null
pkill -9 -f player_simulator 2>/dev/null
pkill -9 -f redis-server 2>/dev/null
sleep 0.5

# 1. Start Redis
"$REDIS_BIN" --port 6379 --daemonize yes --logfile /tmp/redis_m11.log
sleep 0.8
"$REDIS_CLI" ping > /dev/null || { echo "ERROR: Redis not running"; exit 1; }

# 2. Check MySQL
mysql -u game_user -pgame_pass -h 127.0.0.1 -D game_server -e "SELECT 1;" > /dev/null
if [ $? -ne 0 ]; then echo "ERROR: MySQL not reachable"; exit 1; fi

echo "=== Infrastructure Verified: Redis + MySQL 8.0 Ready ==="

# 3. Execute Orchestration Suite
cd "$REPO"
python3 "$REPO/python/orchestrator.py" --port $PORT --repo "$REPO"
ORCH_EXIT=$?

# 4. Verify test_runs table in MySQL
echo ""
echo "=== Direct MySQL test_runs Table Records ==="
mysql -u game_user -pgame_pass -h 127.0.0.1 -D game_server -e "
SELECT run_id, test_name, concurrent_users, requests_per_second, avg_latency_ms, p99_latency_ms, created_at FROM test_runs ORDER BY created_at DESC LIMIT 5;
"

"$REDIS_CLI" shutdown nosave 2>/dev/null
exit $ORCH_EXIT
