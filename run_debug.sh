#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
REPO="/mnt/c/sriya/Multiplayer game server"
PORT=7782

pkill -f game_server 2>/dev/null
sleep 0.3

"$REPO/build/server/game_server" --port $PORT > /tmp/gs_debug.log 2>&1 &
PID=$!
sleep 0.5

python3 "$REPO/debug_responses.py"

kill $PID 2>/dev/null
wait $PID 2>/dev/null
