#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"

pkill -9 -f game_server 2>/dev/null
pkill -9 -f "python3.*app.py" 2>/dev/null
sleep 0.5

# Start Game Server with nohup
cd "$REPO"
nohup "$REPO/build/server/game_server" --port 7777 --metrics-port 9100 --enable-redis --redis-host 127.0.0.1 --redis-port 6379 --db-host 127.0.0.1 --db-port 3306 --db-user game_user --db-pass game_pass --db-name game_server > /tmp/srv.log 2>&1 &
sleep 0.8

# Start Web UI Server with nohup
nohup python3 "$REPO/web/app.py" 8080 > /tmp/web.log 2>&1 &
sleep 0.8

echo "=== Game Server & Web Dashboard are LIVE ==="
