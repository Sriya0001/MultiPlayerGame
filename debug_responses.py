#!/usr/bin/env python3
"""Quick response debugger - shows raw server output."""
import socket, struct, json

PORT = 7782

def go(req):
    with socket.create_connection(("127.0.0.1", PORT), timeout=3) as s:
        d = json.dumps(req).encode()
        s.sendall(struct.pack("<I", len(d)) + d)
        h = b""
        while len(h) < 4: h += s.recv(4 - len(h))
        n = struct.unpack("<I", h)[0]
        body = b""
        while len(body) < n: body += s.recv(n - len(body))
        return json.loads(body)

tests = [
    {"action": "JOIN",   "player_id": 1},
    {"action": "JOIN",   "player_id": 2},
    {"action": "MOVE",   "player_id": 1, "x": 300, "y": 400},
    {"action": "MOVE",   "player_id": 1, "x": 9999, "y": -50},
    {"action": "ATTACK", "player_id": 1, "target_id": 2},
    {"action": "ATTACK", "player_id": 1, "target_id": 1},
    {"action": "CHAT",   "player_id": 1, "message": "hello"},
    {"action": "CHAT",   "player_id": 1, "message": ""},
    {"action": "GET_STATE", "player_id": 1},
    {"action": "UPDATE_SCORE", "player_id": 1, "score": 999},
    {"action": "UPDATE_SCORE", "player_id": 1, "score": -5},
    {"action": "LEAVE",  "player_id": 1},
    {"action": "LEAVE",  "player_id": 999},
]

# Use persistent connection
with socket.create_connection(("127.0.0.1", PORT), timeout=3) as s:
    def tx(req):
        d = json.dumps(req).encode()
        s.sendall(struct.pack("<I", len(d)) + d)
        h = b""
        while len(h) < 4: h += s.recv(4 - len(h))
        n = struct.unpack("<I", h)[0]
        body = b""
        while len(body) < n: body += s.recv(n - len(body))
        return json.loads(body)

    for t in tests:
        r = tx(t)
        print(f"REQ: {t}")
        print(f"RSP: {r}")
        print()
