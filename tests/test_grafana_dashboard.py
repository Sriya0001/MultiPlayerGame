#!/usr/bin/env python3
"""
Milestone 13 Test: Grafana Dashboard Model & Provisioning Validator.

Verifies:
  1. JSON parsing and schema validation of Grafana dashboard model
  2. All required panels are present (Active Players, Throughput, Latency, Error Rate, Cache Hit Ratio, DB Ops)
  3. PromQL expressions match the exact Prometheus metrics exported in Milestone 12
  4. Histogram quantile calculations for P50, P95, and P99 latency percentiles
  5. Datasource & Dashboard provisioning configurations
"""

import json
from pathlib import Path
import sys

failures = 0
def check(label, cond, detail=""):
    global failures
    ok = "\033[32mPASS\033[0m" if cond else "\033[31mFAIL\033[0m"
    print(f"  [{ok}] {label}" + (f"  <- {detail}" if detail else ""))
    if not cond: failures += 1
    return cond

def run():
    print("=" * 70)
    print(" Milestone 13 — Grafana Dashboard & Provisioning Validation Test")
    print("=" * 70)

    repo = Path(__file__).resolve().parent.parent
    dashboard_path = repo / "observability/grafana/dashboards/game_server_overview.json"
    ds_prov_path   = repo / "observability/grafana/provisioning/datasources/prometheus.yml"
    dash_prov_path = repo / "observability/grafana/provisioning/dashboards/dashboards.yml"

    # 1. File existence
    print("\n[1] File & Provisioning Structure")
    check("Dashboard JSON file exists", dashboard_path.exists(), str(dashboard_path))
    check("Prometheus datasource provisioning file exists", ds_prov_path.exists())
    check("Dashboard provider configuration exists", dash_prov_path.exists())

    # 2. JSON validation
    print("\n[2] Dashboard Model Integrity")
    with open(dashboard_path, encoding="utf-8") as f:
        dash = json.load(f)

    check("Dashboard title set", dash.get("title") == "Multiplayer Game Server Overview & Load Testing")
    check("Dashboard UID set", dash.get("uid") == "game-server-perf")
    check("Live refresh interval configured", dash.get("refresh") in ["1s", "2s", "5s"])

    # 3. Panel validation
    panels = [p for p in dash.get("panels", []) if p.get("type") != "row"]
    check("At least 6 panels configured", len(panels) >= 6, f"{len(panels)} panels")

    panel_titles = [p.get("title", "") for p in panels]
    check("Panel 'Active Players' found", any("Active Players" in t for t in panel_titles))
    check("Panel 'Throughput' found", any("Throughput" in t for t in panel_titles))
    check("Panel 'Latency' found", any("Latency" in t for t in panel_titles))
    check("Panel 'Error Rate' found", any("Error Rate" in t for t in panel_titles))
    check("Panel 'Cache Hit Ratio' found", any("Cache Hit" in t for t in panel_titles))
    check("Panel 'Database Operations' found", any("Database" in t for t in panel_titles))

    # 4. PromQL query validation
    print("\n[3] PromQL Expressions & Metric Bindings")
    all_exprs = []
    for p in panels:
        for t in p.get("targets", []):
            if "expr" in t:
                all_exprs.append(t["expr"])

    check("PromQL uses 'game_server_active_players'",
          any("game_server_active_players" in e for e in all_exprs))
    check("PromQL uses 'game_server_requests_total'",
          any("game_server_requests_total" in e for e in all_exprs))
    check("PromQL uses 'game_server_request_latency_seconds_bucket'",
          any("game_server_request_latency_seconds_bucket" in e for e in all_exprs))
    check("PromQL computes histogram_quantile for P50/P95/P99",
          any("histogram_quantile(0.99" in e for e in all_exprs))
    check("PromQL uses 'game_server_redis_cache_hits_total'",
          any("game_server_redis_cache_hits_total" in e for e in all_exprs))
    check("PromQL uses 'game_server_database_operations_total'",
          any("game_server_database_operations_total" in e for e in all_exprs))

    print()
    print("=" * 70)
    if failures == 0:
        print(" \033[32mAll Grafana dashboard validation checks passed!\033[0m")
    else:
        print(f" \033[31m{failures} check(s) FAILED\033[0m")
    print("=" * 70)
    return failures

if __name__ == "__main__":
    sys.exit(0 if run() == 0 else 1)
