#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
REDIS_BIN="$HOME/redis-stable/src/redis-server"
REDIS_CLI="$HOME/redis-stable/src/redis-cli"

pkill -9 -f game_server 2>/dev/null
pkill -9 -f player_simulator 2>/dev/null
pkill -9 -f redis-server 2>/dev/null
sleep 0.5

# 1. Start Redis
"$REDIS_BIN" --port 6379 --daemonize yes --logfile /tmp/redis_m16.log
sleep 0.8

# 2. Run Controlled Experiments Engine
cd "$REPO"
python3 "$REPO/python/run_experiments.py" --repo "$REPO" --base-port 7810
EXP_EXIT=$?

cat "$REPO/docs/benchmarks/experiment_results.md"

"$REDIS_CLI" shutdown nosave 2>/dev/null
exit $EXP_EXIT
