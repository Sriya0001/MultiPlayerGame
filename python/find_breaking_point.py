#!/usr/bin/env python3
"""
Breaking Point & Saturation Detector.

Ramps concurrent players in increasing increments (50 -> 100 -> 250 -> 500 -> 750 -> 1000)
to determine the server's maximum sustainable throughput and breaking point.
"""

import subprocess
import json
import time
import argparse
from pathlib import Path

CONCURRENCY_TIERS = [50, 100, 200, 300, 500, 750, 1000]

def main():
    parser = argparse.ArgumentParser(description="Find server saturation breaking point")
    parser.add_argument("--host", default="127.0.0.1", help="Server host")
    parser.add_argument("--port", type=int, default=7777, help="Server port")
    parser.add_argument("--duration", type=int, default=3, help="Duration per tier (s)")
    parser.add_argument("--simulator", default="./build/simulator/player_simulator", help="Simulator binary")
    parser.add_argument("--outdir", default="python/reports", help="Reports dir")
    args = parser.parse_args()

    out_path = Path(args.outdir)
    out_path.mkdir(parents=True, exist_ok=True)

    print("=" * 75)
    print("           BREAKING POINT & SATURATION ANALYSIS ENGINE")
    print("=" * 75)

    results = []
    knee_point = None

    for players in CONCURRENCY_TIERS:
        json_file = out_path / f"ramp_{players}p.json"
        cmd = [
            args.simulator,
            "--host", args.host,
            "--port", str(args.port),
            "--players", str(players),
            "--duration", str(args.duration),
            "--interval", "5",
            "--json-out", str(json_file)
        ]

        print(f"\n>> Testing Concurrency Tier: {players} players ({args.duration}s)...")
        res = subprocess.run(cmd, capture_output=True, text=True)

        if not json_file.exists():
            print(f"  [CRITICAL] Server failed or unreachable at {players} players!")
            knee_point = players
            break

        with open(json_file, "r") as f:
            data = json.load(f)

        rps = data.get("requests_per_second", 0)
        p99 = data.get("p99_latency_ms", 0)
        avg = data.get("avg_latency_ms", 0)

        results.append({
            "players": players,
            "rps": rps,
            "avg_ms": avg,
            "p99_ms": p99
        })

        print(f"   -> RPS: {rps:,.1f} | Avg Latency: {avg:.3f} ms | P99: {p99:.3f} ms")

        # Detect saturation: if P99 spikes > 50ms or RPS drops significantly
        if p99 > 50.0 and knee_point is None:
            knee_point = players
            print(f"   [!] Saturation knee-point detected at ~{players} concurrent players (P99 latency spiked)")

        time.sleep(1)

    print("\n" + "=" * 75)
    print("                      SATURATION PROFILE SUMMARY")
    print("=" * 75)
    print(f"{'Users':>8} | {'Throughput (RPS)':>18} | {'Avg Latency (ms)':>18} | {'P99 Latency (ms)':>18}")
    print("-" * 75)
    for r in results:
        print(f"{r['players']:>8} | {r['rps']:>18.1f} | {r['avg_ms']:>18.3f} | {r['p99_ms']:>18.3f}")
    print("=" * 75)

    if knee_point:
        print(f"\n[Conclusion] Server begins latency degradation near {knee_point} concurrent players on current hardware.")
    else:
        print(f"\n[Conclusion] Server sustained up to {CONCURRENCY_TIERS[-1]} concurrent players within normal latency bounds.")

if __name__ == "__main__":
    main()
