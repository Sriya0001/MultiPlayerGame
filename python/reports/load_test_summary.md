# Load Testing & Performance Benchmark Report

**Generated on:** 2026-08-29 01:56:01

## Concurrency Benchmark Results

| Tier | Users | Total Reqs | Requests/sec (RPS) | Avg Latency (ms) | P50 (ms) | P95 (ms) | P99 (ms) | Max (ms) |
|:-----|------:|-----------:|-------------------:|-----------------:|---------:|---------:|---------:|---------:|
| Baseline | 50 | 23,947 | 4,762.68 | 0.357 | 0.241 | 0.808 | 1.394 | 16.777 |
| Moderate | 100 | 47,528 | 9,352.20 | 0.454 | 0.210 | 0.900 | 7.820 | 28.483 |
| High | 250 | 93,673 | 18,056.51 | 3.305 | 0.752 | 15.336 | 41.633 | 147.659 |
| Stress | 500 | 63,926 | 6,490.56 | 50.621 | 25.332 | 94.900 | 202.058 | 3620.551 |

## Observations & Bottleneck Analysis
- **Throughput scaling**: Evaluates how requests per second scale as player count scales from Baseline (50) to Stress (500).
- **Tail Latency (P99)**: Monitors thread scheduling and synchronization contention under high load.
