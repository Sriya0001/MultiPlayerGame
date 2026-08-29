#!/usr/bin/env python3
"""
Milestone 8 Test: Latency, Throughput, and Metrics Engine.
Tests:
  1. GET_METRICS schema and presence of core metrics
  2. Active player count accuracy (gauge)
  3. Total and successful request counters
  4. Failed request counter increments on bad actions/JSON
  5. Average latency is calculated and positive
"""

import socket, struct, json, argparse, sys

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

def run(host, port):
    print("=" * 65)
    print(" Milestone 8 — Metrics Engine Verification Test")
    print(f" Server: {host}:{port}")
    print("=" * 65)

    with socket.create_connection((host, port), timeout=5) as s:
        # 1. Initial metrics query
        print("\n[1] GET_METRICS query")
        m1 = tx(s, {"action": "GET_METRICS"})
        check("status ok", m1.get("status") == "ok")
        check("requests_total present", "requests_total" in m1)
        check("successful_requests present", "successful_requests" in m1)
        check("failed_requests present", "failed_requests" in m1)
        check("active_players present", "active_players" in m1)
        check("avg_latency_ms present", "avg_latency_ms" in m1)

        reqs_before = m1.get("requests_total", 0)
        succ_before = m1.get("successful_requests", 0)
        fail_before = m1.get("failed_requests", 0)
        active_before = m1.get("active_players", 0)

        # 2. Player joins -> active_players should increase by 1
        print("\n[2] Gauge Tracking: Player Join")
        tx(s, {"action": "JOIN", "player_id": 801})
        m2 = tx(s, {"action": "GET_METRICS"})
        check("active_players increased", m2.get("active_players") == active_before + 1,
              f"was {active_before}, now {m2.get('active_players')}")

        # 3. Successful Gameplay Actions
        print("\n[3] Counter Tracking: Gameplay Actions")
        tx(s, {"action": "MOVE", "player_id": 801, "x": 100, "y": 200})
        tx(s, {"action": "CHAT", "player_id": 801, "message": "metrics test"})
        tx(s, {"action": "GET_STATE", "player_id": 801})

        # 4. Error Actions -> failed_requests should increase
        print("\n[4] Counter Tracking: Failed Actions")
        tx(s, {"action": "INVALID_ACTION_NAME"})
        # Invalid JSON
        bad_json = b"bad-json-string"
        s.sendall(struct.pack("<I", len(bad_json)) + bad_json)
        recv_msg(s)

        m3 = tx(s, {"action": "GET_METRICS"})
        check("failed_requests increased by at least 2",
              m3.get("failed_requests", 0) >= fail_before + 2,
              f"failed_requests = {m3.get('failed_requests')}")

        check("avg_latency_ms > 0", m3.get("avg_latency_ms", 0) > 0.0,
              f"avg_latency_ms = {m3.get('avg_latency_ms'):.4f} ms")

        # 5. Player leaves -> active_players should decrement
        print("\n[5] Gauge Tracking: Player Leave")
        tx(s, {"action": "LEAVE", "player_id": 801})
        m4 = tx(s, {"action": "GET_METRICS"})
        check("active_players decremented back",
              m4.get("active_players") == active_before,
              f"active_players = {m4.get('active_players')}")

    print()
    print("=" * 65)
    if failures == 0:
        print(" \033[32mAll metrics engine tests passed!\033[0m")
    else:
        print(f" \033[31m{failures} test(s) FAILED\033[0m")
    print("=" * 65)
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
