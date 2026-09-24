#!/usr/bin/env python3
"""
Milestone 11: End-to-End Test Orchestrator & Experiment Engine.

Coordinates the complete full-stack architecture:
  1. Verifies Redis & MySQL services
  2. Builds server & simulator targets
  3. Launches C++ Game Server with Redis & MySQL enabled
  4. Runs graduated benchmark matrix (Baseline, Moderate, High, Stress)
  5. Records all results into MySQL `test_runs` table and disk JSON reports
"""

import subprocess
import time
import json
import argparse
import sys
from pathlib import Path

TIERS = [
    {"name": "baseline", "players": 50,  "duration": 5, "interval": 10},
    {"name": "moderate", "players": 100, "duration": 5, "interval": 10},
    {"name": "high",     "players": 250, "duration": 5, "interval": 10},
    {"name": "stress",   "players": 500, "duration": 5, "interval": 10},
]

def check_service(name: str, test_cmd: list) -> bool:
    res = subprocess.run(test_cmd, capture_output=True, text=True)
    return res.returncode == 0

def save_run_to_mysql(run_data: dict):
    sql = f"""
    INSERT INTO test_runs (
        run_id, test_name, concurrent_users, duration_seconds,
        total_requests, successful_requests, failed_requests,
        requests_per_second, avg_latency_ms, p50_latency_ms,
        p95_latency_ms, p99_latency_ms, max_latency_ms, error_rate_percent
    ) VALUES (
        '{run_data["run_id"]}', '{run_data["name"]}', {run_data["players"]}, {run_data["duration"]},
        {run_data["total_requests"]}, {run_data["successful_requests"]}, {run_data["failed_requests"]},
        {run_data["rps"]}, {run_data["avg_latency_ms"]}, {run_data["p50_latency_ms"]},
        {run_data["p95_latency_ms"]}, {run_data["p99_latency_ms"]}, {run_data["max_latency_ms"]},
        {run_data["error_rate"]}
    );
    """
    cmd = [
        "mysql", "-u", "game_user", "-pgame_pass",
        "-h", "127.0.0.1", "-P", "3306",
        "-D", "game_server", "-e", sql
    ]
    subprocess.run(cmd, capture_output=True)

def main():
    parser = argparse.ArgumentParser(description="Multiplayer Game Server Test Orchestrator")
    parser.add_argument("--port", type=int, default=7777, help="Server port")
    parser.add_argument("--repo", default=".", help="Repository root")
    parser.add_argument("--outdir", default="python/reports", help="Reports output directory")
    args = parser.parse_args()

    repo = Path(args.repo)
    outdir = repo / args.outdir
    outdir.mkdir(parents=True, exist_ok=True)

    print("=" * 70)
    print("      END-TO-END LOAD TESTING ORCHESTRATOR & ANALYSIS PIPELINE")
    print("=" * 70)

    # 1. Health Checks
    print("\n[Step 1] Verifying Infrastructure Services...")
    redis_ok = check_service("Redis", ["python3", str(repo / "tests/check_redis.py"), "6379"])
    mysql_ok = check_service("MySQL", ["mysql", "-u", "game_user", "-pgame_pass", "-h", "127.0.0.1", "-e", "SELECT 1;"])

    redis_status = "\033[32mACTIVE\033[0m" if redis_ok else "\033[31mOFFLINE\033[0m"
    mysql_status = "\033[32mACTIVE\033[0m" if mysql_ok else "\033[31mOFFLINE\033[0m"
    print(f"  - Redis (port 6379): {redis_status}")
    print(f"  - MySQL (port 3306): {mysql_status}")

    if not redis_ok or not mysql_ok:
        print("[ERROR] Services not ready. Please ensure Redis and MySQL are running.")
        sys.exit(1)

    # 2. Build
    print("\n[Step 2] Compiling C++ Binaries with CMake...")
    b_res = subprocess.run(["cmake", "--build", str(repo / "build"), "--parallel", "4"], capture_output=True, text=True)
    if b_res.returncode != 0:
        print(f"[ERROR] Build failed:\n{b_res.stderr}")
        sys.exit(1)
    print("  \033[32mBuild Succeeded.\033[0m")

    # 3. Launch Server
    server_bin = repo / "build/server/game_server"
    simulator_bin = repo / "build/simulator/player_simulator"

    server_cmd = [
        str(server_bin),
        "--port", str(args.port),
        "--threads", "256",
        "--enable-redis",
        "--redis-host", "127.0.0.1",
        "--redis-port", "6379",
        "--db-host", "127.0.0.1",
        "--db-port", "3306",
        "--db-user", "game_user",
        "--db-pass", "game_pass",
        "--db-name", "game_server"
    ]

    print(f"\n[Step 3] Launching Full-Stack Game Server on port {args.port}...")
    server_proc = subprocess.Popen(server_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.0)

    results = []
    run_timestamp = int(time.time())

    try:
        # 4. Run Benchmark Tiers
        print("\n[Step 4] Executing Graduated Load Benchmark Tiers...")
        for tier in TIERS:
            name = tier["name"]
            players = tier["players"]
            duration = tier["duration"]
            interval = tier["interval"]
            run_id = f"run_{run_timestamp}_{name}_{players}p"
            json_file = outdir / f"{run_id}.json"

            print(f"\n  >> Tier: {name.upper():<8} | {players} Players | {duration}s Duration...")

            sim_cmd = [
                str(simulator_bin),
                "--port", str(args.port),
                "--players", str(players),
                "--duration", str(duration),
                "--interval", str(interval),
                "--json-out", str(json_file)
            ]
            t0 = time.time()
            subprocess.run(sim_cmd, capture_output=True)
            elapsed = time.time() - t0

            if not json_file.exists():
                print(f"    [!] Run failed for {name}")
                continue

            with open(json_file) as f:
                data = json.load(f)

            summary = {
                "run_id": run_id,
                "name": name,
                "players": players,
                "duration": duration,
                "total_requests": data.get("total_requests", 0),
                "successful_requests": data.get("successful_requests", 0),
                "failed_requests": data.get("failed_requests", 0),
                "rps": data.get("requests_per_second", 0),
                "avg_latency_ms": data.get("avg_latency_ms", 0),
                "p50_latency_ms": data.get("p50_latency_ms", 0),
                "p95_latency_ms": data.get("p95_latency_ms", 0),
                "p99_latency_ms": data.get("p99_latency_ms", 0),
                "max_latency_ms": data.get("max_latency_ms", 0),
                "error_rate": data.get("error_rate_percent", 0)
            }

            # Persist to MySQL test_runs table
            save_run_to_mysql(summary)
            results.append(summary)

            print(f"     Throughput: {summary['rps']:,.1f} req/s | Avg Latency: {summary['avg_latency_ms']:.3f} ms | P99: {summary['p99_latency_ms']:.3f} ms")
            time.sleep(1)

    finally:
        server_proc.terminate()
        server_proc.wait()

    # 5. Run Analyzer
    print("\n[Step 5] Triggering Data Analysis & Visualization Engine...")
    summary_json = outdir / f"orchestration_summary_{run_timestamp}.json"
    with open(summary_json, "w") as f:
        json.dump(results, f, indent=2)

    subprocess.run(["python3", str(repo / "python/analyze_results.py"), "--input", str(summary_json)])

if __name__ == "__main__":
    main()
