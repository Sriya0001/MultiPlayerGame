#include "server/Metrics.h"
#include <iomanip>
#include <sstream>

MetricsRegistry& MetricsRegistry::instance() {
    static MetricsRegistry s_instance;
    return s_instance;
}

MetricsRegistry::MetricsRegistry()
    : m_latencyBucketCounts(m_latencyBuckets.size() + 1)
{
    reset();
}

void MetricsRegistry::reset() {
    m_requestsTotal.store(0);
    m_successfulRequests.store(0);
    m_failedRequests.store(0);
    m_activePlayers.store(0);
    m_latencyCount.store(0);
    m_latencySumSeconds.store(0.0);
    for (auto& b : m_latencyBucketCounts) {
        b.store(0);
    }
    m_redisCacheHits.store(0);
    m_redisCacheMisses.store(0);
    m_dbOperationsTotal.store(0);
    m_dbOperationsSuccess.store(0);
    m_dbOperationsFailed.store(0);
    m_dbLatencySumSeconds.store(0.0);
}

void MetricsRegistry::incrementRequestsTotal(const std::string&) {
    m_requestsTotal.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::incrementSuccessfulRequests(const std::string&) {
    m_successfulRequests.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::incrementFailedRequests(const std::string&) {
    m_failedRequests.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::setActivePlayers(int count) {
    m_activePlayers.store(count, std::memory_order_relaxed);
}

void MetricsRegistry::incrementActivePlayers() {
    m_activePlayers.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::decrementActivePlayers() {
    m_activePlayers.fetch_sub(1, std::memory_order_relaxed);
}

void MetricsRegistry::recordRequestLatency(double seconds) {
    m_latencyCount.fetch_add(1, std::memory_order_relaxed);

    // Atomic double addition via compare-exchange
    double currentSum = m_latencySumSeconds.load(std::memory_order_relaxed);
    while (!m_latencySumSeconds.compare_exchange_weak(currentSum, currentSum + seconds,
                                                      std::memory_order_relaxed)) {
        // loop until successful
    }

    // Update cumulative histogram buckets
    for (size_t i = 0; i < m_latencyBuckets.size(); ++i) {
        if (seconds <= m_latencyBuckets[i]) {
            m_latencyBucketCounts[i].fetch_add(1, std::memory_order_relaxed);
        }
    }
    // +Inf bucket (always counts all requests)
    m_latencyBucketCounts.back().fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::incrementRedisCacheHits() {
    m_redisCacheHits.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::incrementRedisCacheMisses() {
    m_redisCacheMisses.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::recordDatabaseOperation(double latencySeconds, bool success) {
    m_dbOperationsTotal.fetch_add(1, std::memory_order_relaxed);
    if (success) {
        m_dbOperationsSuccess.fetch_add(1, std::memory_order_relaxed);
    } else {
        m_dbOperationsFailed.fetch_add(1, std::memory_order_relaxed);
    }

    double cur = m_dbLatencySumSeconds.load(std::memory_order_relaxed);
    while (!m_dbLatencySumSeconds.compare_exchange_weak(cur, cur + latencySeconds,
                                                        std::memory_order_relaxed)) {
    }
}

std::string MetricsRegistry::toPrometheusText() const {
    std::ostringstream ss;

    // 1. active_players
    ss << "# HELP game_server_active_players Number of currently connected active players\n"
       << "# TYPE game_server_active_players gauge\n"
       << "game_server_active_players " << m_activePlayers.load() << "\n\n";

    // 2. requests_total
    ss << "# HELP game_server_requests_total Total number of gameplay requests received\n"
       << "# TYPE game_server_requests_total counter\n"
       << "game_server_requests_total " << m_requestsTotal.load() << "\n\n";

    // 3. successful_requests
    ss << "# HELP game_server_successful_requests Total successful gameplay requests\n"
       << "# TYPE game_server_successful_requests counter\n"
       << "game_server_successful_requests " << m_successfulRequests.load() << "\n\n";

    // 4. failed_requests
    ss << "# HELP game_server_failed_requests Total failed or invalid gameplay requests\n"
       << "# TYPE game_server_failed_requests counter\n"
       << "game_server_failed_requests " << m_failedRequests.load() << "\n\n";

    // 5. request_latency_seconds histogram
    ss << "# HELP game_server_request_latency_seconds Request processing turnaround latency\n"
       << "# TYPE game_server_request_latency_seconds histogram\n";
    
    for (size_t i = 0; i < m_latencyBuckets.size(); ++i) {
        ss << "game_server_request_latency_seconds_bucket{le=\"" 
           << std::fixed << std::setprecision(5) << m_latencyBuckets[i] << "\"} "
           << m_latencyBucketCounts[i].load() << "\n";
    }
    ss << "game_server_request_latency_seconds_bucket{le=\"+Inf\"} " 
       << m_latencyBucketCounts.back().load() << "\n";
    ss << "game_server_request_latency_seconds_sum " 
       << std::fixed << std::setprecision(6) << m_latencySumSeconds.load() << "\n";
    ss << "game_server_request_latency_seconds_count " 
       << m_latencyCount.load() << "\n\n";

    // 6. redis metrics
    ss << "# HELP game_server_redis_cache_hits_total Cache hit count\n"
       << "# TYPE game_server_redis_cache_hits_total counter\n"
       << "game_server_redis_cache_hits_total " << m_redisCacheHits.load() << "\n\n";

    ss << "# HELP game_server_redis_cache_misses_total Cache miss count\n"
       << "# TYPE game_server_redis_cache_misses_total counter\n"
       << "game_server_redis_cache_misses_total " << m_redisCacheMisses.load() << "\n\n";

    // 7. database metrics
    ss << "# HELP game_server_database_operations_total Total database queries executed\n"
       << "# TYPE game_server_database_operations_total counter\n"
       << "game_server_database_operations_total " << m_dbOperationsTotal.load() << "\n\n";

    return ss.str();
}

nlohmann::json MetricsRegistry::toJson() const {
    uint64_t total = m_requestsTotal.load();
    uint64_t count = m_latencyCount.load();
    double sum = m_latencySumSeconds.load();
    double avgMs = (count > 0) ? (sum / count * 1000.0) : 0.0;

    return {
        {"active_players",       m_activePlayers.load()},
        {"requests_total",       total},
        {"successful_requests",  m_successfulRequests.load()},
        {"failed_requests",      m_failedRequests.load()},
        {"avg_latency_ms",       avgMs},
        {"redis_cache_hits",     m_redisCacheHits.load()},
        {"redis_cache_misses",   m_redisCacheMisses.load()},
        {"database_operations",  m_dbOperationsTotal.load()}
    };
}
