# Controlled Experiments & Performance Analysis Report

## 1. Experiment 1: Concurrency Scaling Sweep

| Concurrent Players | Throughput (req/s) | Avg Latency (ms) | P50 (ms) | P95 (ms) | P99 Tail (ms) |
|:---|---:|---:|---:|---:|---:|
| **20** | 1,585.1 | 1.499 | 0.286 | 0.647 | 0.910 |
| **50** | 3,226.3 | 2.953 | 0.249 | 0.650 | 0.952 |
| **100** | 4,778.7 | 5.688 | 0.244 | 0.822 | 1.414 |
| **200** | 4,222.0 | 13.532 | 0.311 | 1.622 | 3.377 |

## 2. Experiment 2: Thread Pool Sizing Sweep

| Worker Threads | Throughput (req/s) | Avg Latency (ms) | P99 Tail (ms) | Note |
|:---|---:|---:|---:|:---|
| **4 threads** | 350.1 | 64.469 | 1.219 | Multi-core thread pool |
| **16 threads** | 1,161.3 | 13.002 | 0.978 | Multi-core thread pool |
| **64 threads** | 3,232.4 | 2.970 | 0.943 | Multi-core thread pool |
| **128 threads** | 2,053.1 | 3.397 | 0.935 | Multi-core thread pool |

## 3. Experiment 3: Cache Performance Impact (Redis ON vs OFF)

| Configuration | Throughput (req/s) | Avg Latency (ms) | P99 Tail (ms) | Throughput Gain |
|:---|---:|---:|---:|---:|
| **Redis OFF (No Cache)** | 3,140.4 | 3.136 | 1.587 | +0.0% |
| **Redis ON** | 3,024.2 | 3.816 | 0.923 | -3.7% |

## 4. Experiment 4: Action Mix & Request Frequency

| Scenario | Throughput (req/s) | Avg Latency (ms) | P99 Tail (ms) |
|:---|---:|---:|---:|
| **High Frequency (5ms Interval)** | 6,147.1 | 1.722 | 0.931 |
| **Standard Frequency (20ms Interval)** | 1,635.4 | 5.823 | 1.197 |
