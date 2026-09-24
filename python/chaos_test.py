#!/usr/bin/env python3
"""
Milestone 15: Chaos Engineering & Fault Injection Test Suite.

Simulates real-world infrastructure and network failures:
  1. Scenario 1: Sudden Redis Outage during peak traffic (Graceful Cache Failover)
  2. Scenario 2: Malformed Packet Fuzzing Storm (Protocol & Payload Resilience)
  3. Scenario 3: Massive Abrupt TCP RST Client Disconnect Storm (Connection Cleanup)
"""

import socket
import struct
import json
import time
import subprocess
import argparse
import sys
import random

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

def test_redis_failover(host, game_port, redis_cli):
    print("\n[Scenario 1] In-Flight Cache Outage (Chaos Redis Failure)")
    print("  -> Testing server resilience when Redis goes offline during active gameplay...")

    # Join players
    socks = []
    for i in range(10):
        s = socket.create_connection((host, game_port), timeout=3)
        tx(s, {"action": "JOIN", "player_id": 1000 + i})
        socks.append(s)

    # Move players with Redis running
    for i, s in enumerate(socks):
        tx(s, {"action": "MOVE", "player_id": 1000 + i, "x": 100 + i, "y": 100 + i})

    # KILL REDIS
    print("  -> [CHAOS INJECTION] Killing Redis server now...")
    try:
        with socket.create_connection(("127.0.0.1", 6379), timeout=2) as rs:
            rs.sendall(b"SHUTDOWN NOSAVE\r\n")
    except Exception:
        pass
    try:
        subprocess.run([redis_cli, "shutdown", "nosave"], capture_output=True)
    except Exception:
        pass
    time.sleep(0.5)

    # Send gameplay traffic after Redis is dead
    degraded_success = 0
    for i, s in enumerate(socks):
        try:
            r = tx(s, {"action": "MOVE", "player_id": 1000 + i, "x": 200 + i, "y": 200 + i})
            if r.get("status") == "ok":
                degraded_success += 1
        except Exception as e:
            pass

    check("Server survived sudden Redis crash without killing client connections",
          degraded_success == len(socks),
          f"{degraded_success}/{len(socks)} requests succeeded in degraded mode")

    # Clean up sockets
    for i, s in enumerate(socks):
        try:
            tx(s, {"action": "LEAVE", "player_id": 1000 + i})
            s.close()
        except: pass

def test_packet_fuzzing(host, game_port):
    print("\n[Scenario 2] Malformed Packet & Protocol Fuzzing Storm")
    print("  -> Blasting corrupted/fuzzed packets into the server...")

    handled_count = 0
    total_fuzzed = 0

    with socket.create_connection((host, game_port), timeout=3) as s:
        # Join player first
        tx(s, {"action": "JOIN", "player_id": 9999})

        # 1. Invalid actions
        for i in range(50):
            send_msg(s, {"action": "UNKNOWN_OPCODE_" + str(i), "player_id": 9999})
            r = recv_msg(s)
            if "status" in r: handled_count += 1
            total_fuzzed += 1

        # 2. Missing required fields
        for _ in range(50):
            send_msg(s, {"action": "MOVE"}) # missing fields
            r = recv_msg(s)
            if "status" in r: handled_count += 1
            total_fuzzed += 1

        # 3. Giant out-of-bounds numbers (should clamp)
        for _ in range(50):
            send_msg(s, {"action": "MOVE", "player_id": 9999, "x": 999999999, "y": -999999999})
            r = recv_msg(s)
            if "status" in r: handled_count += 1
            total_fuzzed += 1

        # 4. Raw invalid non-JSON binary payloads
        for _ in range(50):
            garbage = bytes([random.randint(0, 255) for _ in range(32)])
            s.sendall(struct.pack("<I", len(garbage)) + garbage)
            r = recv_msg(s)
            if r.get("status") == "error": handled_count += 1
            total_fuzzed += 1

        tx(s, {"action": "LEAVE", "player_id": 9999})

    check(f"Server safely handled all {total_fuzzed} fuzzed packets with structured responses",
          handled_count == total_fuzzed,
          f"{handled_count}/{total_fuzzed} responses handled cleanly")

def test_client_rst_storm(host, game_port):
    print("\n[Scenario 3] Simultaneous TCP RST Disconnection Storm")
    print("  -> Spawning 50 concurrent clients, joining players, then issuing abrupt TCP RST drops...")

    socks = []
    for i in range(50):
        s = socket.create_connection((host, game_port), timeout=3)
        tx(s, {"action": "JOIN", "player_id": 2000 + i})
        socks.append(s)

    # Abruptly drop all 50 connections with TCP RST (SO_LINGER 0)
    for s in socks:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.close()

    time.sleep(0.8)

    # Connect a fresh client to verify server is fully responsive and unharmed
    with socket.create_connection((host, game_port), timeout=3) as s:
        r = tx(s, {"action": "JOIN", "player_id": 99999})
        check("Server accepted new client immediately after 50-connection RST storm",
              r.get("status") == "ok")
        tx(s, {"action": "LEAVE", "player_id": 99999})

def run(host, game_port, redis_cli):
    print("=" * 72)
    print(" Milestone 15 — Fault Injection & Chaos Resilience Testing Suite")
    print(f" Target Server: {host}:{game_port}")
    print("=" * 72)

    test_redis_failover(host, game_port, redis_cli)
    test_packet_fuzzing(host, game_port)
    test_client_rst_storm(host, game_port)

    print()
    print("=" * 72)
    if failures == 0:
        print(" \033[32mAll Chaos & Fault Injection resilience scenarios passed!\033[0m")
    else:
        print(f" \033[31m{failures} scenario(s) FAILED\033[0m")
    print("=" * 72)
    return failures

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7801)
    ap.add_argument("--redis-cli", default="/home/sriya/redis-stable/src/redis-cli")
    args = ap.parse_args()
    sys.exit(0 if run(args.host, args.port, args.redis_cli) == 0 else 1)
