#!/usr/bin/env python3
"""
Milestone 2 smoke test — tests persistent connections with length-prefix framing.

Protocol: 4-byte little-endian uint32 length header + JSON payload.

Run from WSL:
    python3 test_milestone2.py [--host 127.0.0.1] [--port 7777]
"""

import socket
import struct
import json
import argparse
import sys
import time

# ── Wire protocol helpers ─────────────────────────────────────────────────────

def send_message(sock: socket.socket, payload: dict) -> None:
    """Encode and send a length-prefixed JSON message."""
    data = json.dumps(payload).encode("utf-8")
    header = struct.pack("<I", len(data))   # little-endian uint32
    sock.sendall(header + data)

def recv_message(sock: socket.socket) -> dict:
    """Receive and decode a length-prefixed JSON message."""
    # Read 4-byte header
    header = b""
    while len(header) < 4:
        chunk = sock.recv(4 - len(header))
        if not chunk:
            raise ConnectionError("Server disconnected while reading header")
        header += chunk

    (length,) = struct.unpack("<I", header)

    # Read payload
    payload = b""
    while len(payload) < length:
        chunk = sock.recv(length - len(payload))
        if not chunk:
            raise ConnectionError("Server disconnected while reading payload")
        payload += chunk

    return json.loads(payload.decode("utf-8"))

# ── Test helpers ──────────────────────────────────────────────────────────────

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"

def check(label: str, response: dict, expected_status: str = "ok",
          expected_action: str | None = None) -> bool:
    ok = response.get("status") == expected_status
    if expected_action:
        ok = ok and response.get("action") == expected_action
    mark = PASS if ok else FAIL
    print(f"  [{mark}] {label}")
    if not ok:
        print(f"         got: {response}")
    return ok

# ── Test cases ────────────────────────────────────────────────────────────────

def run_tests(host: str, port: int) -> int:
    """Returns number of failures."""
    failures = 0

    print("=" * 55)
    print(" Milestone 2 Smoke Test")
    print(f" Server: {host}:{port}")
    print("=" * 55)

    # ── Test 1: Single persistent connection, multiple messages ───────────────
    print("\n[Test 1] Persistent connection — all 7 actions")
    with socket.create_connection((host, port), timeout=5) as s:
        cases = [
            ("JOIN",         {"action": "JOIN",         "player_id": 1}),
            ("MOVE",         {"action": "MOVE",         "player_id": 1, "x": 10, "y": 20}),
            ("ATTACK",       {"action": "ATTACK",       "player_id": 1, "target_id": 2}),
            ("CHAT",         {"action": "CHAT",         "player_id": 1, "message": "hi"}),
            ("GET_STATE",    {"action": "GET_STATE",    "player_id": 1}),
            ("UPDATE_SCORE", {"action": "UPDATE_SCORE", "player_id": 1, "score": 99}),
            ("LEAVE",        {"action": "LEAVE",        "player_id": 1}),
        ]
        for label, req in cases:
            send_message(s, req)
            resp = recv_message(s)
            if not check(label, resp, expected_action=label):
                failures += 1

    # ── Test 2: Multiple messages on one connection ───────────────────────────
    print("\n[Test 2] 20 consecutive MOVE messages on one connection")
    with socket.create_connection((host, port), timeout=5) as s:
        send_message(s, {"action": "JOIN", "player_id": 2})
        recv_message(s)

        ok_count = 0
        for i in range(20):
            send_message(s, {"action": "MOVE", "player_id": 2, "x": i, "y": i})
            resp = recv_message(s)
            if resp.get("status") == "ok":
                ok_count += 1

        label = f"All 20 MOVEs returned ok (got {ok_count}/20)"
        ok = ok_count == 20
        print(f"  [{'PASS' if ok else 'FAIL'}] {label}")
        if not ok:
            failures += 1

    # ── Test 3: Error handling on persistent connection ───────────────────────
    print("\n[Test 3] Error handling — invalid JSON and unknown action")
    with socket.create_connection((host, port), timeout=5) as s:
        # Invalid JSON (send raw bytes manually)
        bad = b"not-json-at-all"
        s.sendall(struct.pack("<I", len(bad)) + bad)
        resp = recv_message(s)
        if not check("Invalid JSON → error", resp, expected_status="error"):
            failures += 1

        # Unknown action (connection still alive after previous error)
        send_message(s, {"action": "TELEPORT", "player_id": 1})
        resp = recv_message(s)
        if not check("Unknown action → error", resp, expected_status="error"):
            failures += 1

        # Connection still usable after errors
        send_message(s, {"action": "JOIN", "player_id": 3})
        resp = recv_message(s)
        if not check("Connection alive after errors", resp, expected_action="JOIN"):
            failures += 1

    # ── Test 4: Two separate connections ──────────────────────────────────────
    print("\n[Test 4] Two independent connections simultaneously")
    with socket.create_connection((host, port), timeout=5) as s1:
        # NOTE: server is still single-threaded in M2, so open s2 after s1
        # sends its first message and gets a response (not blocked).
        # True concurrent test comes in Milestone 5 with the thread pool.
        send_message(s1, {"action": "JOIN", "player_id": 10})
        r1 = recv_message(s1)
        if not check("Connection 1 JOIN", r1, expected_action="JOIN"):
            failures += 1

    with socket.create_connection((host, port), timeout=5) as s2:
        send_message(s2, {"action": "JOIN", "player_id": 20})
        r2 = recv_message(s2)
        if not check("Connection 2 JOIN", r2, expected_action="JOIN"):
            failures += 1

    # ── Summary ───────────────────────────────────────────────────────────────
    print()
    print("=" * 55)
    if failures == 0:
        print(f" \033[32mAll tests passed!\033[0m")
    else:
        print(f" \033[31m{failures} test(s) FAILED\033[0m")
    print("=" * 55)
    return failures


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7777)
    args = parser.parse_args()

    try:
        failures = run_tests(args.host, args.port)
        sys.exit(0 if failures == 0 else 1)
    except ConnectionRefusedError:
        print(f"\n[ERROR] Could not connect to {args.host}:{args.port}")
        print("        Is the server running? ./build/server/game_server --port 7777 --debug")
        sys.exit(2)
