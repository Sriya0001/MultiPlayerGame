#!/usr/bin/env python3
"""
Milestone 16: Controlled Experiments & Comparative Analysis Engine.

Automates the 4 required scientific experiments:
  1. Experiment 1: Concurrency Scaling Sweep (20, 50, 100, 200 players)
  2. Experiment 2: Thread Pool Sizing Sweep (4, 16, 64, 128 threads @ 50 players)
  3. Experiment 3: Cache Performance (Redis ON vs Redis OFF @ 50 players)
  4. Experiment 4: Action Mix & Frequency (5ms High Frequency vs 20ms Standard)

Generates comparative ASCII tables and exports docs/benchmarks/experiment_results.md.
"""

import subprocess
import time
import json
import argparse
import sys
from pathlib import Path

def launch_server(repo, port, threads=128, enable_redis=True):
    cmd = [
        str(repo / "build/server/game_server"),
        "--port", str(port),
        "--threads", str(threads),
        "--metrics-port", str(port + 1000)
    ]
    if enable_redis:
        cmd.extend(["--enable-redis", "--redis-host", "127.0.0.1", "--redis-port", "6379"])
    cmd.extend(["--db-host", "127.0.0.1", "--db-port", "3306", "--db-user", "game_user", "--db-pass", "game_pass", "--db-name", "game_server"])

    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.8)
    return proc

def run_sim(repo, port, players, duration=3, interval=10):
    cmd = [
        str(repo / "build/simulator/player_simulator"),
        "--port", str(port),
        "--players", str(players),
        "--duration", str(duration),
        "--interval", str(interval)
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)

    rps = 0.0
    avg_l = 0.0
    p50_l = 0.0
    p95_l = 0.0
    p99_l = 0.0
    total_reqs = 0
    err_rate = 0.0

    for line in res.stdout.split("\n"):
        if "Throughput (RPS):" in line:
            rps = float(line.split(":")[1].replace("req/sec", "").strip())
        elif "Avg Latency:" in line:
            avg_l = float(line.split(":")[1].replace("ms", "").strip())
        elif "P50 (Median) Latency:" in line:
            p50_l = float(line.split(":")[1].replace("ms", "").strip())
        elif "P95 Latency:" in line:
            p95_l = float(line.split(":")[1].replace("ms", "").strip())
        elif "P99 Latency:" in line:
            p99_l = float(line.split(":")[1].replace("ms", "").strip())
        elif "Total Requests:" in line:
            total_reqs = int(line.split(":")[1].strip())
        elif "Error Rate:" in line:
            err_rate = float(line.split(":")[1].replace("%", "").strip())

    return {
        "players": players,
        "total_requests": total_reqs,
        "rps": rps,
        "avg_ms": avg_l,
        "p50_ms": p50_l,
        "p95_ms": p95_l,
        "p99_ms": p99_l,
        "error_rate": err_rate
    }

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=".")
    parser.add_argument("--base-port", type=int, default=7810)
    args = parser.parse_args()

    repo = Path(args.repo).resolve()
    docs_dir = repo / "docs/benchmarks"
    docs_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 80, flush=True)
    print("      MULTI-DIMENSIONAL CONTROLLED EXPERIMENTS & PERFORMANCE BENCHMARKING", flush=True)
    print("=" * 80, flush=True)

    # ── Experiment 1: Concurrency Scaling ─────────────────────────────────────
    print("\n[Experiment 1] Concurrency Scaling Sweep (20 -> 50 -> 100 -> 200 players)", flush=True)
    p1 = launch_server(repo, args.base_port, threads=256, enable_redis=True)
    exp1_results = []
    try:
        for n in [20, 50, 100, 200]:
            r = run_sim(repo, args.base_port, players=n, duration=3)
            exp1_results.append(r)
            print(f"  Players: {n:>3} | RPS: {r['rps']:>8,.1f} | Avg: {r['avg_ms']:>6.3f} ms | P99: {r['p99_ms']:>6.3f} ms", flush=True)
    finally:
        p1.terminate()
        p1.wait()

    # ── Experiment 2: Thread Pool Sizing Sweep ────────────────────────────────
    print("\n[Experiment 2] Thread Pool Sizing Sweep (4 -> 16 -> 64 -> 128 threads @ 50 players)", flush=True)
    exp2_results = []
    for threads in [4, 16, 64, 128]:
        port = args.base_port + 1
        p2 = launch_server(repo, port, threads=threads, enable_redis=True)
        try:
            r = run_sim(repo, port, players=50, duration=3)
            r["threads"] = threads
            exp2_results.append(r)
            print(f"  Threads: {threads:>3} | RPS: {r['rps']:>8,.1f} | Avg: {r['avg_ms']:>6.3f} ms | P99: {r['p99_ms']:>6.3f} ms", flush=True)
        finally:
            p2.terminate()
            p2.wait()

    # ── Experiment 3: Redis Cache (ON vs OFF) ──────────────────────────────────
    print("\n[Experiment 3] Redis Cache Performance Comparison (Redis ON vs OFF @ 50 players)", flush=True)
    exp3_results = []
    for enable_redis in [False, True]:
        port = args.base_port + 2
        p3 = launch_server(repo, port, threads=128, enable_redis=enable_redis)
        mode = "Redis ON" if enable_redis else "Redis OFF (No Cache)"
        try:
            r = run_sim(repo, port, players=50, duration=3)
            r["mode"] = mode
            exp3_results.append(r)
            print(f"  Mode: {mode:<22} | RPS: {r['rps']:>8,.1f} | Avg: {r['avg_ms']:>6.3f} ms | P99: {r['p99_ms']:>6.3f} ms", flush=True)
        finally:
            p3.terminate()
            p3.wait()

    # ── Experiment 4: Action Mix & Frequency ───────────────────────────────────
    print("\n[Experiment 4] Action Frequency Analysis (@ 50 players)", flush=True)
    p4 = launch_server(repo, args.base_port + 3, threads=128, enable_redis=True)
    exp4_results = []
    try:
        r = run_sim(repo, args.base_port + 3, players=50, duration=3, interval=5)
        r["test"] = "High Frequency (5ms Interval)"
        exp4_results.append(r)
        print(f"  High Frequency (5ms)   | RPS: {r['rps']:>8,.1f} | Avg: {r['avg_ms']:>6.3f} ms | P99: {r['p99_ms']:>6.3f} ms", flush=True)
        
        r_std = run_sim(repo, args.base_port + 3, players=50, duration=3, interval=20)
        r_std["test"] = "Standard Frequency (20ms Interval)"
        exp4_results.append(r_std)
        print(f"  Standard (20ms)        | RPS: {r_std['rps']:>8,.1f} | Avg: {r_std['avg_ms']:>6.3f} ms | P99: {r_std['p99_ms']:>6.3f} ms", flush=True)
    finally:
        p4.terminate()
        p4.wait()

    # ── Export Markdown Report ────────────────────────────────────────────────
    report_md = docs_dir / "experiment_results.md"
    with open(report_md, "w") as f:
        f.write("# Controlled Experiments & Performance Analysis Report\n\n")
        f.write("## 1. Experiment 1: Concurrency Scaling Sweep\n\n")
        f.write("| Concurrent Players | Throughput (req/s) | Avg Latency (ms) | P50 (ms) | P95 (ms) | P99 Tail (ms) |\n")
        f.write("|:---|---:|---:|---:|---:|---:|\n")
        for r in exp1_results:
            f.write(f"| **{r['players']}** | {r['rps']:,.1f} | {r['avg_ms']:.3f} | {r['p50_ms']:.3f} | {r['p95_ms']:.3f} | {r['p99_ms']:.3f} |\n")

        f.write("\n## 2. Experiment 2: Thread Pool Sizing Sweep\n\n")
        f.write("| Worker Threads | Throughput (req/s) | Avg Latency (ms) | P99 Tail (ms) | Note |\n")
        f.write("|:---|---:|---:|---:|:---|\n")
        for r in exp2_results:
            f.write(f"| **{r['threads']} threads** | {r['rps']:,.1f} | {r['avg_ms']:.3f} | {r['p99_ms']:.3f} | Multi-core thread pool |\n")

        f.write("\n## 3. Experiment 3: Cache Performance Impact (Redis ON vs OFF)\n\n")
        f.write("| Configuration | Throughput (req/s) | Avg Latency (ms) | P99 Tail (ms) | Throughput Gain |\n")
        f.write("|:---|---:|---:|---:|---:|\n")
        base_rps = exp3_results[0]['rps'] or 1.0
        for r in exp3_results:
            gain = ((r['rps'] - base_rps) / base_rps) * 100.0
            f.write(f"| **{r['mode']}** | {r['rps']:,.1f} | {r['avg_ms']:.3f} | {r['p99_ms']:.3f} | {gain:+.1f}% |\n")

        f.write("\n## 4. Experiment 4: Action Mix & Request Frequency\n\n")
        f.write("| Scenario | Throughput (req/s) | Avg Latency (ms) | P99 Tail (ms) |\n")
        f.write("|:---|---:|---:|---:|\n")
        for r in exp4_results:
            f.write(f"| **{r['test']}** | {r['rps']:,.1f} | {r['avg_ms']:.3f} | {r['p99_ms']:.3f} |\n")

    print(f"\n[SUCCESS] Comprehensive experiment results saved to: {report_md}", flush=True)

if __name__ == "__main__":
    main()
