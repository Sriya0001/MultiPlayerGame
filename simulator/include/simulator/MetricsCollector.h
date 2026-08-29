#pragma once

#include <vector>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <string>

struct RequestMetric {
    std::string action;
    uint64_t    latencyMicros;
    bool        success;
};

struct PerformanceSummary {
    uint64_t totalRequests{0};
    uint64_t successfulRequests{0};
    uint64_t failedRequests{0};
    double   durationSeconds{0.0};
    double   requestsPerSec{0.0};
    double   avgLatencyMs{0.0};
    double   p50LatencyMs{0.0};
    double   p95LatencyMs{0.0};
    double   p99LatencyMs{0.0};
    double   maxLatencyMs{0.0};
    double   errorRatePercent{0.0};
};

class MetricsCollector {
public:
    MetricsCollector() = default;

    void record(const std::string& action, uint64_t latencyMicros, bool success);

    void recordBatch(const std::vector<RequestMetric>& batch);

    PerformanceSummary computeSummary(double durationSec) const;

    void printSummary(const PerformanceSummary& summary) const;

private:
    mutable std::mutex         m_mutex;
    std::vector<RequestMetric> m_metrics;
};
