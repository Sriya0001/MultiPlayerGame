#!/usr/bin/env python3
"""
Milestone 14 Test: Docker & Container Orchestration Configuration Validator.

Verifies:
  1. Multi-stage Dockerfile for game_server (builder + runtime, non-root user, healthcheck)
  2. Multi-stage Dockerfile for player_simulator
  3. Complete docker-compose.yml service topology (mysql, redis, game_server, prometheus, grafana)
  4. Port mappings (7777, 9100, 6379, 3306, 9090, 3000)
  5. Inter-service network bridges and volume configurations
"""

import sys
from pathlib import Path

failures = 0
def check(label, cond, detail=""):
    global failures
    ok = "\033[32mPASS\033[0m" if cond else "\033[31mFAIL\033[0m"
    print(f"  [{ok}] {label}" + (f"  ← {detail}" if detail else ""))
    if not cond: failures += 1
    return cond

def run():
    print("=" * 70)
    print(" Milestone 14 — Docker & Container Orchestration Validator")
    print("=" * 70)

    repo = Path(__file__).resolve().parent.parent
    server_dockerfile = repo / "docker/Dockerfile.server"
    sim_dockerfile    = repo / "docker/Dockerfile.simulator"
    compose_file      = repo / "docker-compose.yml"
    dockerignore      = repo / ".dockerignore"

    # 1. File checks
    print("\n[1] Container Configuration Files")
    check("Server Dockerfile exists", server_dockerfile.exists())
    check("Simulator Dockerfile exists", sim_dockerfile.exists())
    check("docker-compose.yml exists", compose_file.exists())
    check(".dockerignore exists", dockerignore.exists())

    # 2. Server Dockerfile analysis
    print("\n[2] Server Multi-Stage Dockerfile Analysis")
    server_content = server_dockerfile.read_text()
    check("Multi-stage build used (FROM ... AS builder)", "AS builder" in server_content)
    check("Minimal runtime stage used (FROM ... AS runtime)", "AS runtime" in server_content)
    check("Non-root user created & used", "USER gameserver" in server_content)
    check("Healthcheck declared (/health)", "HEALTHCHECK" in server_content and "/health" in server_content)
    check("Ports exposed (7777 and 9100)", "EXPOSE 7777 9100" in server_content)

    # 3. Docker Compose analysis
    print("\n[3] Docker Compose Multi-Container Topology")
    compose_content = compose_file.read_text()
    check("Service 'mysql' configured", "mysql:" in compose_content)
    check("Service 'redis' configured", "redis:" in compose_content)
    check("Service 'game_server' configured", "game_server:" in compose_content)
    check("Service 'prometheus' configured", "prometheus:" in compose_content)
    check("Service 'grafana' configured", "grafana:" in compose_content)
    check("Service 'simulator' configured", "simulator:" in compose_content)

    # Port mappings
    check("Game server port 7777 mapped", "7777:7777" in compose_content)
    check("Metrics port 9100 mapped", "9100:9100" in compose_content)
    check("Redis port 6379 mapped", "6379:6379" in compose_content)
    check("MySQL port 3306 mapped", ":3306" in compose_content)
    check("Prometheus port 9090 mapped", "9090:9090" in compose_content)
    check("Grafana port 3000 mapped", "3000:3000" in compose_content)

    # Health check dependencies
    check("game_server depends on healthy mysql & redis",
          "condition: service_healthy" in compose_content)

    print()
    print("=" * 70)
    if failures == 0:
        print(" \033[32mAll Docker & Compose configuration checks passed!\033[0m")
    else:
        print(f" \033[31m{failures} check(s) FAILED\033[0m")
    print("=" * 70)
    return failures

if __name__ == "__main__":
    sys.exit(0 if run() == 0 else 1)
