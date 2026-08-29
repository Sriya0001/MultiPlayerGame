#!/usr/bin/env python3
"""
Single Scenario Test Runner.
Usage:
    python run_test.py --players 100 --duration 10 [--port 7777]
"""

import subprocess
import argparse
import sys
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="Run single load test scenario")
    parser.add_argument("--host", default="127.0.0.1", help="Server hostname")
    parser.add_argument("--port", type=int, default=7777, help="Server port")
    parser.add_argument("--players", type=int, default=50, help="Number of concurrent virtual players")
    parser.add_argument("--duration", type=int, default=10, help="Duration in seconds")
    parser.add_argument("--interval", type=int, default=10, help="Interval between player actions (ms)")
    parser.add_argument("--simulator", default="./build/simulator/player_simulator", help="Simulator binary path")
    parser.add_argument("--json-out", default="", help="Optional JSON output file path")
    args = parser.parse_args()

    cmd = [
        args.simulator,
        "--host", args.host,
        "--port", str(args.port),
        "--players", str(args.players),
        "--duration", str(args.duration),
        "--interval", str(args.interval)
    ]

    if args.json_out:
        cmd.extend(["--json-out", args.json_out])

    print(f"[run_test] Executing: {' '.join(cmd)}")
    res = subprocess.run(cmd)
    sys.exit(res.returncode)

if __name__ == "__main__":
    main()
