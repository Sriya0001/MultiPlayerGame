#include "simulator/MetricsCollector.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <numeric>

void MetricsCollector::record(const std::string& action, uint64_t latencyMicros, bool success) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics.push_back({action, latencyMicros, success});
}

void MetricsCollector::recordBatch(const std::vector<RequestMetric>& batch) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics.insert(m_metrics.end(), batch.begin(), batch.end());
}

PerformanceSummary MetricsCollector::computeSummary(double durationSec) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    PerformanceSummary summary;
    summary.totalRequests = m_metrics.size();
    summary.durationSeconds = durationSec > 0.0 ? durationSec : 0.001;

    if (m_metrics.empty()) {
        return summary;
    }

    std::vector<uint64_t> latencies;
    latencies.reserve(m_metrics.size());
    uint64_t totalMicros = 0;

    for (const auto& m : m_metrics) {
        if (m.success) {
            summary.successfulRequests++;
        } else {
            summary.failedRequests++;
        }
        latencies.push_back(m.latencyMicros);
        totalMicros += m.latencyMicros;
    }

    summary.requestsPerSec = static_cast<double>(summary.totalRequests) / summary.durationSeconds;
    summary.errorRatePercent = (summary.totalRequests > 0)
        ? (100.0 * static_cast<double>(summary.failedRequests) / static_cast<double>(summary.totalRequests))
        : 0.0;

    summary.avgLatencyMs = (static_cast<double>(totalMicros) / m_metrics.size()) / 1000.0;

    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();

    auto getPercentileMs = [&](double pct) -> double {
        if (n == 0) return 0.0;
        size_t idx = static_cast<size_t>((pct / 100.0) * n);
        if (idx >= n) idx = n - 1;
        return static_cast<double>(latencies[idx]) / 1000.0;
    };

    summary.p50LatencyMs = getPercentileMs(50.0);
    summary.p95LatencyMs = getPercentileMs(95.0);
    summary.p99LatencyMs = getPercentileMs(99.0);
    summary.maxLatencyMs = static_cast<double>(latencies.back()) / 1000.0;

    return summary;
}

void MetricsCollector::printSummary(const PerformanceSummary& s) const {
    std::cout << "\n"
              << "==============================================================\n"
              << "            SIMULATOR PERFORMANCE BENCHMARK REPORT            \n"
              << "==============================================================\n"
              << std::left  << std::setw(28) << "  Total Duration:"       << std::right << std::fixed << std::setprecision(2) << s.durationSeconds << " s\n"
              << std::left  << std::setw(28) << "  Total Requests:"       << std::right << s.totalRequests << "\n"
              << std::left  << std::setw(28) << "  Successful Requests:"  << std::right << s.successfulRequests << "\n"
              << std::left  << std::setw(28) << "  Failed Requests:"      << std::right << s.failedRequests << "\n"
              << std::left  << std::setw(28) << "  Error Rate:"           << std::right << std::fixed << std::setprecision(2) << s.errorRatePercent << " %\n"
              << "--------------------------------------------------------------\n"
              << std::left  << std::setw(28) << "  Throughput (RPS):"     << std::right << std::fixed << std::setprecision(2) << s.requestsPerSec << " req/sec\n"
              << std::left  << std::setw(28) << "  Avg Latency:"          << std::right << std::fixed << std::setprecision(3) << s.avgLatencyMs << " ms\n"
              << std::left  << std::setw(28) << "  P50 (Median) Latency:" << std::right << std::fixed << std::setprecision(3) << s.p50LatencyMs << " ms\n"
              << std::left  << std::setw(28) << "  P95 Latency:"          << std::right << std::fixed << std::setprecision(3) << s.p95LatencyMs << " ms\n"
              << std::left  << std::setw(28) << "  P99 Latency:"          << std::right << std::fixed << std::setprecision(3) << s.p99LatencyMs << " ms\n"
              << std::left  << std::setw(28) << "  Max Latency:"          << std::right << std::fixed << std::setprecision(3) << s.maxLatencyMs << " ms\n"
              << "==============================================================\n\n";
}
