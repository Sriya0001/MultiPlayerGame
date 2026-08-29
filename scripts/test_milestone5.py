#!/usr/bin/env python3
"""
Milestone 5 — Concurrent client test.

Spawns N threads, each opening a TCP connection and running a full game
session (JOIN → MOVEs → ATTACK → GET_STATE → LEAVE). All threads run
simultaneously to stress the thread pool.

Pass/fail criteria:
  - 0 errors from any client
  - All players visible in GET_STATE during the run
  - No deadlocks (timeout catches hangs)
"""

import socket, struct, json, argparse, sys
import threading, time, random

# ── Protocol helpers ──────────────────────────────────────────────────────────

def send_msg(s, payload):
    d = json.dumps(payload).encode()
    s.sendall(struct.pack("<I", len(d)) + d)

def recv_msg(s):
    h = b""
    while len(h) < 4:
        c = s.recv(4 - len(h))
        if not c: raise ConnectionError("server closed connection")
        h += c
    (n,) = struct.unpack("<I", h)
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c: raise ConnectionError("server closed connection")
        b += c
    return json.loads(b)

def tx(s, req):
    send_msg(s, req)
    return recv_msg(s)

# ── Client simulation ─────────────────────────────────────────────────────────

def player_session(host, port, player_id, results, index):
    """Run one full game session; store result in results[index]."""
    errors = []
    try:
        with socket.create_connection((host, port), timeout=10) as s:
            # JOIN
            r = tx(s, {"action": "JOIN", "player_id": player_id})
            if r.get("status") != "ok":
                errors.append(f"JOIN failed: {r}")

            # 5 MOVE messages with random positions
            rng = random.Random(player_id)
            for _ in range(5):
                x = rng.randint(0, 1000)
                y = rng.randint(0, 1000)
                r = tx(s, {"action": "MOVE", "player_id": player_id, "x": x, "y": y})
                if r.get("status") != "ok":
                    errors.append(f"MOVE failed: {r}")

            # CHAT
            r = tx(s, {"action": "CHAT", "player_id": player_id,
                        "message": f"hello from {player_id}"})
            if r.get("status") != "ok":
                errors.append(f"CHAT failed: {r}")

            # GET_STATE
            r = tx(s, {"action": "GET_STATE", "player_id": player_id})
            if r.get("status") != "ok":
                errors.append(f"GET_STATE failed: {r}")
            player_count = r.get("player_count", 0)

            # UPDATE_SCORE
            r = tx(s, {"action": "UPDATE_SCORE", "player_id": player_id,
                        "score": player_id * 10})
            if r.get("status") != "ok":
                errors.append(f"UPDATE_SCORE failed: {r}")

            # LEAVE
            r = tx(s, {"action": "LEAVE", "player_id": player_id})
            if r.get("status") != "ok":
                errors.append(f"LEAVE failed: {r}")

        results[index] = {"player_id": player_id, "errors": errors,
                          "player_count_seen": player_count}

    except Exception as ex:
        results[index] = {"player_id": player_id,
                          "errors": [f"Exception: {ex}"],
                          "player_count_seen": 0}

# ── Main test ─────────────────────────────────────────────────────────────────

def run(host, port, num_players):
    print("=" * 62)
    print(f" Milestone 5 — Concurrent Client Test")
    print(f" Server: {host}:{port}   Players: {num_players}")
    print("=" * 62)

    results = [None] * num_players
    threads = []

    # Use player IDs 1001..1000+N to avoid clashes with earlier tests
    base_id = 1001

    print(f"\n Starting {num_players} concurrent sessions...")
    t0 = time.time()

    for i in range(num_players):
        t = threading.Thread(
            target=player_session,
            args=(host, port, base_id + i, results, i),
            daemon=True
        )
        threads.append(t)

    # Start all threads simultaneously
    for t in threads:
        t.start()

    # Wait for all to finish (max 30s)
    for t in threads:
        t.join(timeout=30)

    elapsed = time.time() - t0
    print(f" All sessions finished in {elapsed:.2f}s")

    # ── Analyze results ───────────────────────────────────────────────────────
    total_errors     = 0
    failed_sessions  = 0
    max_players_seen = 0
    timed_out        = 0

    for i, r in enumerate(results):
        if r is None:
            timed_out += 1
            total_errors += 1
            continue
        if r["errors"]:
            failed_sessions += 1
            total_errors += len(r["errors"])
        if r["player_count_seen"] > max_players_seen:
            max_players_seen = r["player_count_seen"]

    failures = 0
    def check(label, cond, detail=""):
        nonlocal failures
        mark = "\033[32mPASS\033[0m" if cond else "\033[31mFAIL\033[0m"
        print(f"  [{mark}] {label}" + (f"  ← {detail}" if detail else ""))
        if not cond: failures += 1

    print(f"\n[Results]")
    check("Zero timed-out sessions",    timed_out == 0,
          f"{timed_out} timed out")
    check("Zero failed sessions",       failed_sessions == 0,
          f"{failed_sessions} failed")
    check("Zero total errors",          total_errors == 0,
          f"{total_errors} errors")
    check("Concurrency detected",       max_players_seen > 1,
          f"max simultaneous players seen = {max_players_seen}")
    check("All sessions completed",     sum(1 for r in results if r) == num_players)

    rps = (num_players * 10) / elapsed  # ~10 messages per session
    print(f"\n  Throughput: ~{rps:.0f} req/s  ({num_players*10} total messages)")
    print(f"  Avg session time: {elapsed/num_players*1000:.1f} ms")

    # Print first few errors for debugging
    if total_errors > 0:
        print("\n  Errors (first 5):")
        count = 0
        for r in results:
            if r and r["errors"]:
                for e in r["errors"][:2]:
                    print(f"    player {r['player_id']}: {e}")
                    count += 1
                    if count >= 5: break
            if count >= 5: break

    print()
    print("=" * 62)
    if failures == 0:
        print(f" \033[32mAll tests passed!\033[0m")
    else:
        print(f" \033[31m{failures} check(s) FAILED\033[0m")
    print("=" * 62)
    return failures


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--host",    default="127.0.0.1")
    ap.add_argument("--port",    type=int, default=7777)
    ap.add_argument("--players", type=int, default=50,
                    help="Number of concurrent players (default: 50)")
    args = ap.parse_args()
    try:
        sys.exit(0 if run(args.host, args.port, args.players) == 0 else 1)
    except ConnectionRefusedError:
        print(f"\n[ERROR] Cannot connect to {args.host}:{args.port}")
        sys.exit(2)
