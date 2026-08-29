#!/usr/bin/env python3
"""
Milestone 10 Test: Real MySQL 8.0 Persistence Test.

Verifies:
  1. Real MySQL 8.0 connection on port 3306
  2. Players table insertion & high score updates on JOIN/LEAVE
  3. Game events persistence into game_events table
  4. Direct MySQL verification
"""

import socket, struct, json, argparse, sys, subprocess

def send_msg(s, payload):
    d = json.dumps(payload).encode()
    s.sendall(struct.pack("<I", len(d)) + d)

def recv_msg(s):
    h = b""
    while len(h) < 4:
        c = s.recv(4 - len(h))
        if not c: raise ConnectionError("closed")
        h += c
    (n,) = struct.unpack("<I", h)
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c: raise ConnectionError("closed")
        b += c
    return json.loads(b)

def tx(s, req):
    send_msg(s, req)
    return recv_msg(s)

failures = 0
def check(label, cond, detail=""):
    global failures
    ok = "\033[32mPASS\033[0m" if cond else "\033[31mFAIL\033[0m"
    print(f"  [{ok}] {label}" + (f"  ← {detail}" if detail else ""))
    if not cond: failures += 1
    return cond

def query_mysql(sql):
    cmd = [
        "mysql", "-u", "game_user", "-pgame_pass",
        "-h", "127.0.0.1", "-P", "3306",
        "-D", "game_server", "-s", "-N", "-e", sql
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    return res.stdout.strip()

def run(host, port):
    print("=" * 68)
    print(" Milestone 10 — Real MySQL 8.0 Persistence Test")
    print(f" Server: {host}:{port}   MySQL: 127.0.0.1:3306")
    print("=" * 68)

    with socket.create_connection((host, port), timeout=5) as s:
        # 1. Gameplay sessions that trigger DB writes
        print("\n[1] Executing Gameplay Session to Trigger MySQL Persistence...")
        r_join = tx(s, {"action": "JOIN", "player_id": 999})
        check("JOIN player 999 ok", r_join.get("status") == "ok")

        tx(s, {"action": "MOVE", "player_id": 999, "x": 300, "y": 400})
        tx(s, {"action": "UPDATE_SCORE", "player_id": 999, "score": 2750})
        
        r_leave = tx(s, {"action": "LEAVE", "player_id": 999})
        check("LEAVE player 999 ok", r_leave.get("status") == "ok")

        # 2. Join players 998 and 997 with elimination
        tx(s, {"action": "JOIN", "player_id": 998})
        tx(s, {"action": "JOIN", "player_id": 997})
        for _ in range(4):
            tx(s, {"action": "ATTACK", "player_id": 998, "target_id": 997})

        tx(s, {"action": "LEAVE", "player_id": 998})

        # Query metrics to verify DB operations were tracked
        m = tx(s, {"action": "GET_METRICS"})
        check("Database operations recorded in metrics",
              m.get("database_operations", 0) > 0,
              f"operations count = {m.get('database_operations')}")

    # 3. Direct MySQL Database Inspection
    print("\n[2] Direct MySQL 8.0 Database Verification")
    
    # Verify player 999 persistence
    p_row = query_mysql("SELECT player_id, score, high_score FROM players WHERE player_id = 999;")
    check("Player 999 row found in MySQL", len(p_row) > 0, p_row)
    if p_row:
        parts = p_row.split()
        check("Player 999 score persisted (2750)", parts[1] == "2750", f"score = {parts[1]}")
        check("Player 999 high_score persisted (2750)", parts[2] == "2750", f"high_score = {parts[2]}")

    # Verify game events in MySQL
    event_count_str = query_mysql("SELECT COUNT(*) FROM game_events WHERE player_id IN (999, 998, 997);")
    event_count = int(event_count_str) if event_count_str.isdigit() else 0
    check("Game events persisted in MySQL game_events table", event_count >= 3, f"total events = {event_count}")

    print()
    print("=" * 68)
    if failures == 0:
        print(" \033[32mAll MySQL 8.0 persistence tests passed!\033[0m")
    else:
        print(f" \033[31m{failures} test(s) FAILED\033[0m")
    print("=" * 68)
    return failures

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7777)
    args = ap.parse_args()
    try:
        sys.exit(0 if run(args.host, args.port) == 0 else 1)
    except ConnectionRefusedError:
        print(f"\n[ERROR] Cannot connect to {args.host}:{args.port}")
        sys.exit(2)
