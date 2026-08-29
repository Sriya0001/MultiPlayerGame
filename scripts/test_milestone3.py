#!/usr/bin/env python3
"""
Milestone 3 smoke test — validates real game logic.

Tests:
  1. JOIN returns player state (x, y, health, score)
  2. MOVE updates position and clamps to map bounds
  3. ATTACK applies damage and awards score
  4. ATTACK eliminates player at 0 health
  5. CHAT validates message content
  6. GET_STATE returns all players
  7. UPDATE_SCORE sets new score
  8. LEAVE returns final score and removes player
  9. Validation errors (missing fields, bad types, duplicates)
"""

import socket, struct, json, argparse, sys

# ── Protocol helpers ──────────────────────────────────────────────────────────

def send_msg(sock, payload: dict) -> None:
    data = json.dumps(payload).encode("utf-8")
    sock.sendall(struct.pack("<I", len(data)) + data)

def recv_msg(sock) -> dict:
    hdr = b""
    while len(hdr) < 4:
        c = sock.recv(4 - len(hdr))
        if not c: raise ConnectionError("disconnected reading header")
        hdr += c
    (n,) = struct.unpack("<I", hdr)
    body = b""
    while len(body) < n:
        c = sock.recv(n - len(body))
        if not c: raise ConnectionError("disconnected reading body")
        body += c
    return json.loads(body)

def transact(sock, req: dict) -> dict:
    send_msg(sock, req)
    return recv_msg(sock)

# ── Assertion helper ──────────────────────────────────────────────────────────

failures = 0

def check(label: str, cond: bool, detail: str = "") -> bool:
    global failures
    mark = "\033[32mPASS\033[0m" if cond else "\033[31mFAIL\033[0m"
    print(f"  [{mark}] {label}" + (f"  ({detail})" if detail else ""))
    if not cond:
        failures += 1
    return cond

def ok(r):   return r.get("status") == "ok"
def err(r):  return r.get("status") == "error"

# ── Tests ─────────────────────────────────────────────────────────────────────

def run(host, port):
    print("=" * 58)
    print(" Milestone 3 — Gameplay Protocol Test")
    print(f" Server: {host}:{port}")
    print("=" * 58)

    with socket.create_connection((host, port), timeout=5) as s:

        # ── 1. JOIN ───────────────────────────────────────────────────────────
        print("\n[1] JOIN")
        r = transact(s, {"action": "JOIN", "player_id": 101})
        check("status ok",          ok(r))
        check("action=JOIN",        r.get("action") == "JOIN")
        check("player_id returned", r.get("player_id") == 101)
        check("health=100",         r.get("health") == 100)
        check("score=0",            r.get("score") == 0)
        check("x returned",         "x" in r)
        check("y returned",         "y" in r)

        # duplicate JOIN
        r2 = transact(s, {"action": "JOIN", "player_id": 101})
        check("duplicate JOIN → error", err(r2))

        # join player 2 for attack tests
        transact(s, {"action": "JOIN", "player_id": 102})

        # ── 2. MOVE ───────────────────────────────────────────────────────────
        print("\n[2] MOVE")
        r = transact(s, {"action": "MOVE", "player_id": 101, "x": 300, "y": 400})
        check("status ok",   ok(r))
        check("x updated",   r.get("x") == 300)
        check("y updated",   r.get("y") == 400)

        # out-of-bounds → clamped (not error)
        r = transact(s, {"action": "MOVE", "player_id": 101, "x": 9999, "y": -50})
        check("out-of-bounds clamped ok", ok(r))
        check("x clamped to 1000",  r.get("x") == 1000)
        check("y clamped to 0",     r.get("y") == 0)

        # missing field
        r = transact(s, {"action": "MOVE", "player_id": 101, "x": 10})
        check("missing y → error", err(r))

        # ── 3. ATTACK ─────────────────────────────────────────────────────────
        print("\n[3] ATTACK")
        r = transact(s, {"action": "ATTACK", "player_id": 101, "target_id": 102})
        check("status ok",           ok(r))
        check("damage=25",           r.get("damage") == 25)
        check("target_health=75",    r.get("target_health") == 75)
        check("attacker_score>0",    r.get("attacker_score", 0) > 0)

        # self-attack
        r = transact(s, {"action": "ATTACK", "player_id": 101, "target_id": 101})
        check("self-attack → error", err(r))

        # attack non-existent target
        r = transact(s, {"action": "ATTACK", "player_id": 101, "target_id": 999})
        check("unknown target → error", err(r))

        # ── 4. ATTACK until elimination ───────────────────────────────────────
        print("\n[4] Elimination (3 more attacks → 0 HP)")
        for _ in range(3):
            transact(s, {"action": "ATTACK", "player_id": 101, "target_id": 102})
        r = transact(s, {"action": "ATTACK", "player_id": 101, "target_id": 102})
        # After elimination target is gone → next attack hits ghost
        # Last attack before ghost: check eliminated flag was set at some point
        # (The 4th attack completes the kill; check target removed)
        check("eliminated target gone", err(r) or r.get("eliminated") is True)

        # ── 5. CHAT ───────────────────────────────────────────────────────────
        print("\n[5] CHAT")
        r = transact(s, {"action": "CHAT", "player_id": 101, "message": "hello!"})
        check("status ok",       ok(r))
        check("message echoed",  r.get("message") == "hello!")

        r = transact(s, {"action": "CHAT", "player_id": 101, "message": ""})
        check("empty message → error", err(r))

        r = transact(s, {"action": "CHAT", "player_id": 101, "message": "x" * 300})
        check("too-long message → error", err(r))

        # ── 6. GET_STATE ──────────────────────────────────────────────────────
        print("\n[6] GET_STATE")
        # add player 103 for richer state
        transact(s, {"action": "JOIN", "player_id": 103})
        r = transact(s, {"action": "GET_STATE", "player_id": 101})
        check("status ok",            ok(r))
        check("player_count >= 2",    r.get("player_count", 0) >= 2)
        check("players is list",      isinstance(r.get("players"), list))
        ids = [p["player_id"] for p in r.get("players", [])]
        check("player 101 in state",  101 in ids)
        check("player 103 in state",  103 in ids)

        # ── 7. UPDATE_SCORE ───────────────────────────────────────────────────
        print("\n[7] UPDATE_SCORE")
        r = transact(s, {"action": "UPDATE_SCORE", "player_id": 101, "score": 9999})
        check("status ok",    ok(r))
        check("score=9999",   r.get("score") == 9999)

        r = transact(s, {"action": "UPDATE_SCORE", "player_id": 101, "score": -1})
        check("negative score → error", err(r))

        # ── 8. LEAVE ──────────────────────────────────────────────────────────
        print("\n[8] LEAVE")
        r = transact(s, {"action": "LEAVE", "player_id": 101})
        check("status ok",          ok(r))
        check("final_score present", "final_score" in r)

        # player 101 gone — verify via GET_STATE as 103
        r = transact(s, {"action": "GET_STATE", "player_id": 103})
        ids = [p["player_id"] for p in r.get("players", [])]
        check("player 101 removed from state", 101 not in ids)

        # LEAVE unknown player
        r = transact(s, {"action": "LEAVE", "player_id": 999})
        check("unknown player LEAVE → error", err(r))

    # ── Summary ───────────────────────────────────────────────────────────────
    print()
    print("=" * 58)
    if failures == 0:
        print(" \033[32mAll tests passed!\033[0m")
    else:
        print(f" \033[31m{failures} test(s) FAILED\033[0m")
    print("=" * 58)
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
        print("        Start server: ./build/server/game_server --port 7777 --debug")
        sys.exit(2)
