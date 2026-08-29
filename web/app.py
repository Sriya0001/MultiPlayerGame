#!/usr/bin/env python3
"""
Lightweight Web Bridge & Gateway for Multiplayer Game Server.

Serves:
  - Visual HTML5 Arena UI on http://localhost:8080
  - REST endpoints bridging web clients to C++ TCP Game Server (port 7777)
"""

import http.server
import socketserver
import socket
import struct
import json
import urllib.parse
import sys
from pathlib import Path

GAME_SERVER_HOST = "127.0.0.1"
GAME_SERVER_PORT = 7777
WEB_PORT = 8080

def send_tcp_msg(host, port, payload):
    try:
        with socket.create_connection((host, port), timeout=2.0) as s:
            data = json.dumps(payload).encode("utf-8")
            s.sendall(struct.pack("<I", len(data)) + data)
            
            h = b""
            while len(h) < 4:
                chunk = s.recv(4 - len(h))
                if not chunk: raise ConnectionError("Closed")
                h += chunk
            (n,) = struct.unpack("<I", h)
            
            body = b""
            while len(body) < n:
                chunk = s.recv(n - len(body))
                if not chunk: raise ConnectionError("Closed")
                body += chunk
            return json.loads(body.decode("utf-8"))
    except Exception as e:
        return {"status": "error", "message": f"Server unreachable: {str(e)}"}

class GameWebHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        self.web_dir = Path(__file__).resolve().parent
        super().__init__(*args, directory=str(self.web_dir), **kwargs)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/api/state":
            res = send_tcp_msg(GAME_SERVER_HOST, GAME_SERVER_PORT, {"action": "GET_STATE"})
            self.send_json(res)
        elif parsed.path == "/api/metrics":
            res = send_tcp_msg(GAME_SERVER_HOST, GAME_SERVER_PORT, {"action": "GET_METRICS"})
            self.send_json(res)
        else:
            super().do_GET()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/api/action":
            content_length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(content_length)
            try:
                action_data = json.loads(body.decode('utf-8'))
                res = send_tcp_msg(GAME_SERVER_HOST, GAME_SERVER_PORT, action_data)
                self.send_json(res)
            except Exception as e:
                self.send_json({"status": "error", "message": str(e)}, status=400)
        else:
            self.send_response(404)
            self.end_headers()

    def send_json(self, data, status=200):
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        # Suppress noisy HTTP request logging in terminal
        pass

def main():
    port = WEB_PORT
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    print("=" * 65)
    print(f"  🎮 Multiplayer Game Server Visual Dashboard & Arena")
    print(f"  🌐 Open your browser at: http://localhost:{port}")
    print(f"  🔌 Connected to C++ Game Server at {GAME_SERVER_HOST}:{GAME_SERVER_PORT}")
    print("=" * 65)
    
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("0.0.0.0", port), GameWebHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down web server.")

if __name__ == "__main__":
    main()
