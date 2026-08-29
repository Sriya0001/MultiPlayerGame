#!/usr/bin/env python3
"""
Interactive Terminal Monitor HUD for Multiplayer Game Server.

Renders an ASCII live telemetry dashboard updating in real-time.
Run with: python3 python/monitor_cli.py
"""

import urllib.request
import json
import time
import os
import sys

def clear_screen():
    sys.stdout.write("\033[2J\033[H")
    sys.stdout.flush()

def fetch_json(url):
    try:
        req = urllib.request.urlopen(url, timeout=1.0)
        return json.loads(req.read().decode("utf-8"))
    except:
        return None

def main():
    base_url = "http://127.0.0.1:8080"
    if len(sys.argv) > 1:
        base_url = sys.argv[1]

    last_reqs = 0
    last_time = time.time()
    current_rps = 0.0

    print("Connecting to Game Server HUD at", base_url, "...")

    try:
        while True:
            metrics = fetch_json(f"{base_url}/api/metrics")
            state = fetch_json(f"{base_url}/api/state")

            now = time.time()
            dt = now - last_time

            if metrics and "requests_total" in metrics:
                total_reqs = metrics.get("requests_total", 0)
                if last_reqs > 0 and dt > 0:
                    current_rps = max(0.0, (total_reqs - last_reqs) / dt)
                last_reqs = total_reqs
                last_time = now

            clear_screen()
            print("┌" + "─" * 74 + "┐")
            print("│   🎮  MULTIPLAYER GAME SERVER — LIVE TELEMETRY & OPERATIONS HUD          │")
            print("├" + "─" * 74 + "┤")

            if not metrics or not state:
                print("│  [!] SERVER OFFLINE / CONNECTING TO HTTP BRIDGE...                       │")
                print("└" + "─" * 74 + "┘")
                time.sleep(1)
                continue

            active_p = metrics.get("active_players", 0)
            cache_hits = metrics.get("redis_cache_hits", 0)
            db_ops = metrics.get("database_operations", 0)
            avg_lat = metrics.get("avg_latency_ms", 0.0)
            match_status = state.get("match_status", "ACTIVE")

            print(f"│  Status: \033[32m● {match_status:<10}\033[0m  Engine: \033[36mC++17 (256 Threads)\033[0m   Observability: \033[33mPrometheus\033[0m │")
            print("├" + "─" * 74 + "┤")
            print(f"│  • Active Entities:      \033[1;32m{active_p:>8}\033[0m   │  • Total Requests:     \033[1;37m{last_reqs:>10,}\033[0m  │")
            print(f"│  • Real-Time Throughput: \033[1;36m{current_rps:>8.1f}\033[0m rps│  • Avg Latency:        \033[1;33m{avg_lat:>9.3f}\033[0m ms│")
            print(f"│  • Redis Cache Hits:     \033[1;34m{cache_hits:>8,}\033[0m   │  • MySQL DB Operations: \033[1;35m{db_ops:>9,}\033[0m  │")
            print("├" + "─" * 74 + "┤")
            print("│  🏆 LIVE MATCH LEADERBOARD                                               │")

            players = state.get("players", [])
            sorted_p = sorted(players, key=lambda x: x.get("score", 0), reverse=True)[:5]
            if not sorted_p:
                print("│     Waiting for active players in arena...                               │")
            else:
                for i, p in enumerate(sorted_p):
                    pid = p.get("player_id", 0)
                    hp = p.get("health", 100)
                    sc = p.get("score", 0)
                    tag = "YOU" if pid == 8888 else f"Bot P{pid}"
                    print(f"│   #{i+1:<2} {tag:<18} HP: {hp:>3}  Score: {sc:>6} pts                        │")

            print("├" + "─" * 74 + "┤")
            print("│  📜 RECENT COMBAT EVENTS                                                 │")
            events = (state.get("recent_events") or state.get("event_log") or [])[-4:]
            if not events:
                print("│     No combat events yet.                                                │")
            else:
                for ev in events:
                    etype = ev.get("event_type", "EVENT")
                    pid = ev.get("player_id", 0)
                    data = ev.get("data", "")[:35]
                    print(f"│   [{etype:<6}] Player {pid:<5} {data:<46} │")

            print("└" + "─" * 74 + "┘")
            print("  Press Ctrl+C to exit interactive monitor.")
            time.sleep(0.5)

    except KeyboardInterrupt:
        print("\nExiting monitor HUD.")

if __name__ == "__main__":
    main()
