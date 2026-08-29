#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <nlohmann/json.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// MetricsRegistry
//
// High-performance thread-safe application metrics registry.
// Uses atomic primitives to ensure zero lock contention in the hot request path.
//
// Tracks:
//   - active_players
//   - requests_total
//   - successful_requests
//   - failed_requests
//   - request_latency_seconds (Prometheus histogram)
//   - redis_cache_hits
//   - redis_cache_misses
//   - database_operations
//   - database_latency_seconds
// ─────────────────────────────────────────────────────────────────────────────

class MetricsRegistry {
public:
    static MetricsRegistry& instance();

    // ── Counters & Gauges ─────────────────────────────────────────────────────
    void incrementRequestsTotal(const std::string& action);
    void incrementSuccessfulRequests(const std::string& action);
    void incrementFailedRequests(const std::string& action);

    void setActivePlayers(int count);
    void incrementActivePlayers();
    void decrementActivePlayers();

    void recordRequestLatency(double seconds);

    // Redis & DB metrics (instrumented in M9 and M10)
    void incrementRedisCacheHits();
    void incrementRedisCacheMisses();
    void recordDatabaseOperation(double latencySeconds, bool success);

    // ── Export formats ────────────────────────────────────────────────────────
    std::string toPrometheusText() const;
    nlohmann::json toJson() const;

    void reset();

private:
    MetricsRegistry();

    // Standard latency histogram bucket boundaries in seconds
    const std::vector<double> m_latencyBuckets = {
        0.0001, // 0.1ms
        0.00025,// 0.25ms
        0.0005, // 0.5ms
        0.001,  // 1ms
        0.0025, // 2.5ms
        0.005,  // 5ms
        0.010,  // 10ms
        0.025,  // 25ms
        0.050,  // 50ms
        0.100,  // 100ms
        0.250,  // 250ms
        0.500,  // 500ms
        1.000   // 1s
    };

    std::atomic<uint64_t> m_requestsTotal{0};
    std::atomic<uint64_t> m_successfulRequests{0};
    std::atomic<uint64_t> m_failedRequests{0};
    std::atomic<int64_t>  m_activePlayers{0};

    // Latency Histogram (sum, count, buckets)
    std::atomic<uint64_t> m_latencyCount{0};
    std::atomic<double>   m_latencySumSeconds{0.0};
    std::vector<std::atomic<uint64_t>> m_latencyBucketCounts;

    // Cache metrics
    std::atomic<uint64_t> m_redisCacheHits{0};
    std::atomic<uint64_t> m_redisCacheMisses{0};

    // DB metrics
    std::atomic<uint64_t> m_dbOperationsTotal{0};
    std::atomic<uint64_t> m_dbOperationsSuccess{0};
    std::atomic<uint64_t> m_dbOperationsFailed{0};
    std::atomic<double>   m_dbLatencySumSeconds{0.0};
};

// ─────────────────────────────────────────────────────────────────────────────
// ScopedLatencyTimer
// ─────────────────────────────────────────────────────────────────────────────
class ScopedLatencyTimer {
public:
    ScopedLatencyTimer()
        : m_start(std::chrono::high_resolution_clock::now())
    {}

    ~ScopedLatencyTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(end - m_start).count();
        MetricsRegistry::instance().recordRequestLatency(sec);
    }

private:
    std::chrono::high_resolution_clock::time_point m_start;
};
