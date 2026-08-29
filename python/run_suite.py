#!/usr/bin/env python3
"""
Automated Load Test Suite Orchestrator.

Runs configurable test tiers using the C++ player simulator:
  1. Baseline   (50 players)
  2. Moderate   (100 players)
  3. High       (250 players)
  4. Stress     (500 players)
  5. Saturation / Ramp-up

Collects actual metrics (RPS, Avg/P50/P95/P99/Max Latency, Error Rate)
and formats them into comparison tables and report files.
"""

import subprocess
import os
import sys
import json
import time
import argparse
from pathlib import Path

SCENARIOS = [
    {"name": "Baseline", "players": 50,  "duration": 5, "interval": 10},
    {"name": "Moderate", "players": 100, "duration": 5, "interval": 10},
    {"name": "High",     "players": 250, "duration": 5, "interval": 10},
    {"name": "Stress",   "players": 500, "duration": 5, "interval": 10},
]

def run_scenario(simulator_bin: str, host: str, port: int, scenario: dict, output_dir: Path) -> dict:
    name = scenario["name"]
    players = scenario["players"]
    duration = scenario["duration"]
    interval = scenario["interval"]
    json_path = output_dir / f"{name.lower()}_{players}p.json"

    cmd = [
        simulator_bin,
        "--host", host,
        "--port", str(port),
        "--players", str(players),
        "--duration", str(duration),
        "--interval", str(interval),
        "--json-out", str(json_path)
    ]

    print(f"\n[{name.upper()} SCENARIO] Running {players} concurrent players for {duration}s...")
    t0 = time.time()
    res = subprocess.run(cmd, capture_output=True, text=True)
    wall_time = time.time() - t0

    if not json_path.exists():
        print(f"  [ERROR] Simulator failed to produce JSON output for {name}: {res.stderr}")
        return {
            "name": name,
            "players": players,
            "rps": 0,
            "avg_ms": 0,
            "p50_ms": 0,
            "p95_ms": 0,
            "p99_ms": 0,
            "max_ms": 0,
            "error_rate": 100.0,
            "total_reqs": 0
        }

    with open(json_path, "r") as f:
        data = json.load(f)

    return {
        "name": name,
        "players": players,
        "rps": data.get("requests_per_second", 0),
        "avg_ms": data.get("avg_latency_ms", 0),
        "p50_ms": data.get("p50_latency_ms", 0),
        "p95_ms": data.get("p95_latency_ms", 0),
        "p99_ms": data.get("p99_latency_ms", 0),
        "max_ms": data.get("max_latency_ms", 0),
        "error_rate": data.get("error_rate_percent", 0),
        "total_reqs": data.get("total_requests", 0),
        "wall_time_s": wall_time
    }

def print_summary_table(results: list):
    print("\n" + "=" * 90)
    print("                      LOAD TEST SUITE — PERFORMANCE COMPARISON MATRIX")
    print("=" * 90)
    header = f"{'Tier':<10} | {'Users':>6} | {'Total Req':>10} | {'RPS':>10} | {'Avg (ms)':>9} | {'P50 (ms)':>9} | {'P95 (ms)':>9} | {'P99 (ms)':>9} | {'Max (ms)':>9}"
    print(header)
    print("-" * len(header))
    for r in results:
        print(
            f"{r['name']:<10} | "
            f"{r['players']:>6} | "
            f"{r['total_reqs']:>10} | "
            f"{r['rps']:>10.2f} | "
            f"{r['avg_ms']:>9.3f} | "
            f"{r['p50_ms']:>9.3f} | "
            f"{r['p95_ms']:>9.3f} | "
            f"{r['p99_ms']:>9.3f} | "
            f"{r['max_ms']:>9.3f}"
        )
    print("=" * 90)

def generate_markdown_report(results: list, report_path: Path):
    lines = [
        "# Load Testing & Performance Benchmark Report",
        "",
        f"**Generated on:** {time.strftime('%Y-%m-%d %H:%M:%S')}",
        "",
        "## Concurrency Benchmark Results",
        "",
        "| Tier | Users | Total Reqs | Requests/sec (RPS) | Avg Latency (ms) | P50 (ms) | P95 (ms) | P99 (ms) | Max (ms) |",
        "|:-----|------:|-----------:|-------------------:|-----------------:|---------:|---------:|---------:|---------:|"
    ]
    for r in results:
        lines.append(
            f"| {r['name']} | {r['players']} | {r['total_reqs']:,} | {r['rps']:,.2f} | {r['avg_ms']:.3f} | {r['p50_ms']:.3f} | {r['p95_ms']:.3f} | {r['p99_ms']:.3f} | {r['max_ms']:.3f} |"
        )
    lines.append("")
    lines.append("## Observations & Bottleneck Analysis")
    lines.append("- **Throughput scaling**: Evaluates how requests per second scale as player count scales from Baseline (50) to Stress (500).")
    lines.append("- **Tail Latency (P99)**: Monitors thread scheduling and synchronization contention under high load.")
    lines.append("")

    with open(report_path, "w") as f:
        f.write("\n".join(lines))
    print(f"\n[Report] Generated markdown report saved to: {report_path}")

def main():
    parser = argparse.ArgumentParser(description="Run Automated Load Test Suite")
    parser.add_argument("--host", default="127.0.0.1", help="Server host")
    parser.add_argument("--port", type=int, default=7777, help="Server port")
    parser.add_argument("--simulator", default="./build/simulator/player_simulator", help="Path to player_simulator binary")
    parser.add_argument("--outdir", default="python/reports", help="Directory to save reports")
    args = parser.parse_args()

    out_path = Path(args.outdir)
    out_path.mkdir(parents=True, exist_ok=True)

    results = []
    for sc in SCENARIOS:
        res = run_scenario(args.simulator, args.host, args.port, sc, out_path)
        results.append(res)
        time.sleep(1) # brief cooldown between test levels

    print_summary_table(results)
    generate_markdown_report(results, out_path / "load_test_summary.md")

if __name__ == "__main__":
    main()
