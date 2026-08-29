#!/bin/bash
# Milestone 1 smoke test — run from WSL inside the repo directory
set -e

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER="$REPO_DIR/build/server/game_server"
PORT=7778   # use 7778 to avoid conflicts

echo "============================================"
echo " Milestone 1 Smoke Test"
echo "============================================"

# Start the server in background
"$SERVER" --port $PORT --debug &
SERVER_PID=$!
echo "[INFO] Server started (PID=$SERVER_PID)"
sleep 0.5

run_test() {
    local label="$1"
    local payload="$2"
    echo ""
    echo "--- $label ---"
    echo "  Request:  $payload"
    response=$(echo "$payload" | nc -q1 localhost $PORT 2>/dev/null || echo "(no response)")
    echo "  Response: $response"
}

run_test "JOIN"         '{"action":"JOIN","player_id":1}'
run_test "MOVE"         '{"action":"MOVE","player_id":1,"x":120,"y":240}'
run_test "ATTACK"       '{"action":"ATTACK","player_id":1,"target_id":2}'
run_test "CHAT"         '{"action":"CHAT","player_id":1,"message":"hello"}'
run_test "GET_STATE"    '{"action":"GET_STATE","player_id":1}'
run_test "UPDATE_SCORE" '{"action":"UPDATE_SCORE","player_id":1,"score":99}'
run_test "LEAVE"        '{"action":"LEAVE","player_id":1}'
run_test "Invalid JSON" 'not-valid-json'
run_test "Unknown act." '{"action":"TELEPORT","player_id":1}'

echo ""
echo "============================================"
echo " Stopping server..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
echo " Done."
echo "============================================"
