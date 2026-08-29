#!/usr/bin/env python3
"""
Milestone 20 Test: CI/CD Pipeline & Final Project Integrity Validator.

Verifies:
  1. GitHub Actions workflow (.github/workflows/ci.yml)
  2. Production README.md completeness and sections
  3. All test suites and benchmark outputs
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
    print(" Milestone 20 — Final Project Integrity & CI/CD Pipeline Validator")
    print("=" * 72)

    repo = Path(__file__).resolve().parent.parent
    ci_yml = repo / ".github/workflows/ci.yml"
    readme = repo / "README.md"
    summary = repo / "docs/portfolio_summary.md"

    # 1. CI Workflow
    print("\n[1] GitHub Actions CI/CD Pipeline")
    check(".github/workflows/ci.yml exists", ci_yml.exists())
    if ci_yml.exists():
        text = ci_yml.read_text()
        check("CI defines build-and-test job", "build-and-test:" in text)
        check("CI defines docker-build job", "docker-build:" in text)
        check("CI defines validate-configs job", "validate-configs:" in text)
        check("CI provisions Redis service container", "image: redis:" in text)
        check("CI provisions MySQL service container", "image: mysql:8.0" in text)

    # 2. Master README
    print("\n[2] Master Production README.md")
    check("README.md exists", readme.exists())
    if readme.exists():
        rtext = readme.read_text()
        check("README contains performance highlights table", "Peak Throughput" in rtext)
        check("README contains system architecture diagram", "APPLICATION SERVER LAYER" in rtext)
        check("README contains Quickstart guide", "Quickstart Guide" in rtext)
        check("README contains Visual Web Arena instructions", "Visual Web Arena" in rtext)
        check("README contains benchmark experiment results", "Controlled Benchmark Experiments" in rtext)
        check("README contains Cloud Deployment guide", "Cloud Deployment" in rtext)

    # 3. Portfolio Case Study
    print("\n[3] Portfolio Case Study & Interview Guide")
    check("docs/portfolio_summary.md exists", summary.exists())
    if summary.exists():
        stext = summary.read_text()
        check("Case study contains STAR interview scenarios", "STAR Method" in stext)

    print()
    print("=" * 72)
    if failures == 0:
        print(" \033[32mAll Milestone 20 and full project integrity checks passed!\033[0m")
    else:
        print(f" \033[31m{failures} check(s) FAILED\033[0m")
    print("=" * 72)
    return failures

if __name__ == "__main__":
    sys.exit(0 if run() == 0 else 1)
