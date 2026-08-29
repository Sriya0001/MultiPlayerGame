#!/usr/bin/env python3
import socket, time, sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 6379
connected = False

for _ in range(10):
    try:
        with socket.create_connection(('127.0.0.1', port), timeout=2) as s:
            s.sendall(b"*1\r\n$4\r\nPING\r\n")
            res = s.recv(1024)
            if b"PONG" in res:
                connected = True
                break
    except Exception:
        time.sleep(0.3)

if not connected:
    print(f"[check_redis] Connection to port {port} failed")
    sys.exit(1)

print(f"=== Redis Server Verified on port {port} (PONG) ===")
sys.exit(0)
