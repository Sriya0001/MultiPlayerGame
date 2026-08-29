#!/usr/bin/env python3
"""
Redis Comparative Benchmark.
Measures and compares server performance:
  1. WITHOUT REDIS (In-Memory Fallback)
  2. WITH REDIS    (Session & State Cache)

Metrics:
  - Throughput (RPS)
  - Avg / P50 / P95 / P99 Latency
  - Cache Hits
  - Cache Misses
  - Cache Hit Ratio %
"""

import subprocess
import json
import time
import argparse
from pathlib import Path

def query_metrics(host, port):
    import socket, struct
    try:
        with socket.create_connection((host, port), timeout=3) as s:
            payload = json.dumps({"action": "GET_METRICS"}).encode()
            s.sendall(struct.pack("<I", len(payload)) + payload)
            h = s.recv(4)
            n = struct.unpack("<I", h)[0]
            b = b""
            while len(b) < n: b += s.recv(n - len(b))
            return json.loads(b)
    except Exception as ex:
        print(f"Error querying metrics: {ex}")
        return {}

def run_benchmark(simulator_bin, host, port, players, duration, out_json):
    cmd = [
        simulator_bin,
        "--host", host,
        "--port", str(port),
        "--players", str(players),
        "--duration", str(duration),
        "--interval", "5",
        "--json-out", str(out_json)
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if Path(out_json).exists():
        with open(out_json) as f:
            return json.load(f)
    return {}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7777)
    parser.add_argument("--players", type=int, default=100)
    parser.add_argument("--duration", type=int, default=5)
    parser.add_argument("--simulator", default="./build/simulator/player_simulator")
    parser.add_argument("--out-json", default="/tmp/bench_result.json")
    args = parser.parse_args()

    bench = run_benchmark(args.simulator, args.host, args.port, args.players, args.duration, args.out_json)
    metrics = query_metrics(args.host, args.port)

    hits = metrics.get("redis_cache_hits", 0)
    misses = metrics.get("redis_cache_misses", 0)
    total_cache_ops = hits + misses
    hit_ratio = (100.0 * hits / total_cache_ops) if total_cache_ops > 0 else 0.0

    print("\n" + "=" * 65)
    print(f"            BENCHMARK RESULTS (Players: {args.players}, Duration: {args.duration}s)")
    print("=" * 65)
    print(f"  Throughput (RPS):       {bench.get('requests_per_second', 0):,.2f} req/s")
    print(f"  Avg Latency:            {bench.get('avg_latency_ms', 0):.3f} ms")
    print(f"  P50 Latency:            {bench.get('p50_latency_ms', 0):.3f} ms")
    print(f"  P95 Latency:            {bench.get('p95_latency_ms', 0):.3f} ms")
    print(f"  P99 Latency:            {bench.get('p99_latency_ms', 0):.3f} ms")
    print(f"  Cache Hits:             {hits:,}")
    print(f"  Cache Misses:           {misses:,}")
    print(f"  Cache Hit Ratio:        {hit_ratio:.2f} %")
    print("=" * 65)

if __name__ == "__main__":
    main()
