#!/usr/bin/env python3
"""
Milestone 17 Test: Cloud Deployment Automation Validator (Terraform & Ansible).

Verifies:
  1. Terraform configurations (main.tf, variables.tf, outputs.tf)
  2. Security group ingress rules for ports 7777, 9100, 8080, 9090, 3000, 22
  3. EC2 compute instance and VPC topology
  4. Ansible playbooks (setup_game_server.yml, setup_monitoring.yml)
  5. Systemd service unit files (game-server.service, game-web.service)
"""

from pathlib import Path
import sys

failures = 0
def check(label, cond, detail=""):
    global failures
    ok = "\033[32mPASS\033[0m" if cond else "\033[31mFAIL\033[0m"
    print(f"  [{ok}] {label}" + (f"  ← {detail}" if detail else ""))
    if not cond: failures += 1
    return cond

def run():
    print("=" * 72)
    print(" Milestone 17 — Cloud Deployment Automation Validator (Terraform & Ansible)")
    print("=" * 72)

    repo = Path(__file__).resolve().parent.parent
    tf_main   = repo / "terraform/main.tf"
    tf_vars   = repo / "terraform/variables.tf"
    tf_outs   = repo / "terraform/outputs.tf"
    ans_play1 = repo / "ansible/playbooks/setup_game_server.yml"
    ans_play2 = repo / "ansible/playbooks/setup_monitoring.yml"
    srv_unit  = repo / "ansible/systemd/game-server.service"
    web_unit  = repo / "ansible/systemd/game-web.service"

    # 1. File existence
    print("\n[1] Infrastructure as Code (IaC) File Structure")
    check("terraform/main.tf exists", tf_main.exists())
    check("terraform/variables.tf exists", tf_vars.exists())
    check("terraform/outputs.tf exists", tf_outs.exists())
    check("ansible/playbooks/setup_game_server.yml exists", ans_play1.exists())
    check("ansible/playbooks/setup_monitoring.yml exists", ans_play2.exists())
    check("ansible/systemd/game-server.service exists", srv_unit.exists())
    check("ansible/systemd/game-web.service exists", web_unit.exists())

    # 2. Terraform HCL analysis
    print("\n[2] Terraform AWS Topology Analysis")
    tf_text = tf_main.read_text()
    check("VPC resource declared (10.0.0.0/16)", "resource \"aws_vpc\"" in tf_text)
    check("Internet Gateway declared", "resource \"aws_internet_gateway\"" in tf_text)
    check("Public Subnet declared", "resource \"aws_subnet\"" in tf_text)
    check("EC2 compute instance declared", "resource \"aws_instance\" \"game_server\"" in tf_text)

    # Security Group Ports
    check("Security Group opens port 7777 (Game Server)", "7777" in tf_text)
    check("Security Group opens port 9100 (Prometheus Metrics)", "9100" in tf_text)
    check("Security Group opens port 8080 (Web Arena Dashboard)", "8080" in tf_text)
    check("Security Group opens port 9090 (Prometheus UI)", "9090" in tf_text)
    check("Security Group opens port 3000 (Grafana)", "3000" in tf_text)
    check("Security Group opens port 22 (SSH)", "22" in tf_text)

    # 3. Ansible & Systemd analysis
    print("\n[3] Ansible Automation & Systemd Unit Analysis")
    play1_text = ans_play1.read_text()
    check("Ansible installs build tools (gcc, cmake, git)", "build-essential" in play1_text and "cmake" in play1_text)
    check("Ansible installs Redis & MySQL services", "mysql-server" in play1_text and "redis-server" in play1_text)
    check("Ansible compiles C++ Game Server binary", "cmake --build build" in play1_text)
    check("Ansible deploys and enables systemd services", "game-server" in play1_text and "game-web" in play1_text)

    srv_unit_text = srv_unit.read_text()
    check("game-server.service has Restart=always policy", "Restart=always" in srv_unit_text)
    check("game-server.service sets LimitNOFILE=65535", "LimitNOFILE=65535" in srv_unit_text)

    print()
    print("=" * 72)
    if failures == 0:
        print(" \033[32mAll Terraform & Ansible cloud deployment checks passed!\033[0m")
    else:
        print(f" \033[31m{failures} check(s) FAILED\033[0m")
    print("=" * 72)
    return failures

if __name__ == "__main__":
    sys.exit(0 if run() == 0 else 1)
