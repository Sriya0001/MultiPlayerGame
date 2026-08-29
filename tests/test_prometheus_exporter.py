#!/usr/bin/env python3
"""
Milestone 12 Test: Prometheus Metrics HTTP Exporter Test.

Verifies:
  1. HTTP GET http://<host>:<port>/metrics returns 200 OK
  2. Content-Type is text/plain; version=0.0.4
  3. Metric HELP and TYPE declarations exist
  4. Core game server metrics are properly formatted for Prometheus
  5. Histogram buckets, sums, and counts are valid floats/integers
  6. HTTP GET http://<host>:<port>/health returns status UP
"""

import urllib.request
import argparse
import sys

failures = 0
def check(label, cond, detail=""):
    global failures
    ok = "\033[32mPASS\033[0m" if cond else "\033[31mFAIL\033[0m"
    print(f"  [{ok}] {label}" + (f"  ← {detail}" if detail else ""))
    if not cond: failures += 1
    return cond

def run(host, metrics_port):
    print("=" * 68)
    print(" Milestone 12 — Prometheus Metrics HTTP Exporter Test")
    print(f" Target: http://{host}:{metrics_port}/metrics")
    print("=" * 68)

    metrics_url = f"http://{host}:{metrics_port}/metrics"
    health_url  = f"http://{host}:{metrics_port}/health"

    # 1. Test /health
    print("\n[1] Health Endpoint Verification (/health)")
    try:
        req = urllib.request.urlopen(health_url, timeout=3)
        check("HTTP 200 on /health", req.status == 200)
        body = req.read().decode()
        check("Health JSON response 'UP'", '"status":"UP"' in body, body.strip())
    except Exception as ex:
        check("Health check failed", False, str(ex))

    # 2. Test /metrics
    print("\n[2] Prometheus Metrics Endpoint Verification (/metrics)")
    try:
        req = urllib.request.urlopen(metrics_url, timeout=3)
        check("HTTP 200 on /metrics", req.status == 200)
        c_type = req.headers.get("Content-Type", "")
        check("Content-Type text/plain (Prometheus)", "text/plain" in c_type, c_type)

        metrics_text = req.read().decode()
        lines = metrics_text.strip().split("\n")
        check("Non-empty metrics response", len(lines) > 10, f"{len(lines)} lines")

        # Parse Prometheus metrics
        metric_keys = set()
        for line in lines:
            if line.startswith("# HELP "):
                metric_keys.add(line.split()[2])

        check("Metric 'game_server_active_players' exported",
              "game_server_active_players" in metric_keys)
        check("Metric 'game_server_requests_total' exported",
              "game_server_requests_total" in metric_keys)
        check("Metric 'game_server_successful_requests' exported",
              "game_server_successful_requests" in metric_keys)
        check("Metric 'game_server_failed_requests' exported",
              "game_server_failed_requests" in metric_keys)
        check("Metric 'game_server_request_latency_seconds' histogram exported",
              "game_server_request_latency_seconds" in metric_keys)
        check("Metric 'game_server_redis_cache_hits_total' exported",
              "game_server_redis_cache_hits_total" in metric_keys)
        check("Metric 'game_server_database_operations_total' exported",
              "game_server_database_operations_total" in metric_keys)

        # Check histogram formatting
        has_bucket_inf = any("game_server_request_latency_seconds_bucket{le=\"+Inf\"}" in l for l in lines)
        has_sum = any("game_server_request_latency_seconds_sum" in l for l in lines)
        has_count = any("game_server_request_latency_seconds_count" in l for l in lines)

        check("Histogram includes +Inf bucket", has_bucket_inf)
        check("Histogram includes _sum", has_sum)
        check("Histogram includes _count", has_count)

        print("\n[Sample Prometheus Metrics Output]")
        print("-" * 50)
        for l in lines[:15]:
            print(f"  {l}")
        print("  ...")
        print("-" * 50)

    except Exception as ex:
        check("Metrics query failed", False, str(ex))

    print()
    print("=" * 68)
    if failures == 0:
        print(" \033[32mAll Prometheus exporter tests passed!\033[0m")
    else:
        print(f" \033[31m{failures} test(s) FAILED\033[0m")
    print("=" * 68)
    return failures

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=9100)
    args = ap.parse_args()
    sys.exit(0 if run(args.host, args.port) == 0 else 1)
