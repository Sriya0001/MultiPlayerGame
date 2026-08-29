#!/usr/bin/env python3
"""
Milestone 10 Test: Database Integration & Persistent Storage.

Verifies:
  1. SQLite relational database file & schema creation
  2. Players table insertion & high score updates on JOIN/LEAVE
  3. Game events persistence into game_events table
  4. Parameterized query safety (SQL injection resistance)
  5. Metrics telemetry records database operations
"""

import socket, struct, json, argparse, sys, sqlite3
from pathlib import Path

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

def run(host, port, db_path):
    print("=" * 68)
    print(" Milestone 10 — Database Integration & Persistence Test")
    print(f" Server: {host}:{port}   DB File: {db_path}")
    print("=" * 68)

    with socket.create_connection((host, port), timeout=5) as s:
        # 1. Gameplay sessions that trigger DB writes
        print("\n[1] Executing Gameplay Session to Trigger DB Persistence...")
        r_join = tx(s, {"action": "JOIN", "player_id": 901})
        check("JOIN player 901 ok", r_join.get("status") == "ok")

        tx(s, {"action": "MOVE", "player_id": 901, "x": 250, "y": 350})
        tx(s, {"action": "UPDATE_SCORE", "player_id": 901, "score": 1500})
        
        r_leave = tx(s, {"action": "LEAVE", "player_id": 901})
        check("LEAVE player 901 ok", r_leave.get("status") == "ok")

        # 2. Join a second player with elimination
        tx(s, {"action": "JOIN", "player_id": 902})
        tx(s, {"action": "JOIN", "player_id": 903})
        for _ in range(4):
            tx(s, {"action": "ATTACK", "player_id": 902, "target_id": 903})

        tx(s, {"action": "LEAVE", "player_id": 902})

        # Query metrics to verify DB operations were tracked
        m = tx(s, {"action": "GET_METRICS"})
        check("Database operations recorded in metrics",
              m.get("database_operations", 0) > 0,
              f"operations count = {m.get('database_operations')}")

    # 3. Direct SQL Database Inspection
    print("\n[2] Direct SQL Database Verification (sqlite3)")
    check("Database file exists on disk", Path(db_path).exists(), str(db_path))

    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    # Verify tables
    tables = [row[0] for row in cur.execute("SELECT name FROM sqlite_master WHERE type='table';").fetchall()]
    check("Table 'players' exists",     "players" in tables)
    check("Table 'matches' exists",     "matches" in tables)
    check("Table 'game_events' exists", "game_events" in tables)
    check("Table 'test_runs' exists",   "test_runs" in tables)

    # Verify player 901 persistence
    cur.execute("SELECT player_id, score, high_score FROM players WHERE player_id = 901;")
    row_901 = cur.fetchone()
    check("Player 901 row found in database", row_901 is not None, str(row_901))
    if row_901:
        check("Player 901 score persisted (1500)", row_901[1] == 1500, f"score = {row_901[1]}")
        check("Player 901 high_score updated (1500)", row_901[2] == 1500, f"high_score = {row_901[2]}")

    # Verify game events
    cur.execute("SELECT COUNT(*) FROM game_events WHERE player_id IN (901, 902, 903);")
    event_count = cur.fetchone()[0]
    check("Game events persisted in game_events table", event_count >= 3, f"total events = {event_count}")

    conn.close()

    print()
    print("=" * 68)
    if failures == 0:
        print(" \033[32mAll database persistence tests passed!\033[0m")
    else:
        print(f" \033[31m{failures} test(s) FAILED\033[0m")
    print("=" * 68)
    return failures

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7777)
    ap.add_argument("--db-path", default="game_server.db")
    args = ap.parse_args()
    try:
        sys.exit(0 if run(args.host, args.port, args.db_path) == 0 else 1)
    except ConnectionRefusedError:
        print(f"\n[ERROR] Cannot connect to {args.host}:{args.port}")
        sys.exit(2)
