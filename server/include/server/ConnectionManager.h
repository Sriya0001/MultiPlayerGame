#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// ConnectionInfo
//
// Metadata about a single active TCP connection.
// ─────────────────────────────────────────────────────────────────────────────
struct ConnectionInfo {
    uint64_t    connectionId;
    int         fd;
    std::string remoteAddr;
    std::chrono::steady_clock::time_point connectedAt;

    uint64_t messagesReceived{0};
    uint64_t messagesSent{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// ConnectionManager
//
// Thread-safe registry of active TCP connections.
// Each connection is assigned a monotonically increasing connection ID.
//
// Responsibilities:
//   - Register / unregister connections
//   - Track per-connection message counts
//   - Report active connection count
//
// This class will be shared across worker threads starting in Milestone 5.
// The mutex is in place now so no refactoring is needed then.
// ─────────────────────────────────────────────────────────────────────────────
class ConnectionManager {
public:
    ConnectionManager() = default;

    // Non-copyable
    ConnectionManager(const ConnectionManager&)            = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // Register a new connection. Returns the assigned connection ID.
    uint64_t registerConnection(int fd, const std::string& remoteAddr);

    // Unregister a connection when it closes.
    void unregisterConnection(uint64_t connectionId);

    // Increment the received message counter for a connection.
    void recordMessageReceived(uint64_t connectionId);

    // Increment the sent message counter for a connection.
    void recordMessageSent(uint64_t connectionId);

    // Number of currently active connections.
    std::size_t activeCount() const;

    // Total connections ever registered (including closed).
    uint64_t totalConnections() const;

private:
    mutable std::mutex                              m_mutex;
    std::unordered_map<uint64_t, ConnectionInfo>    m_connections;
    std::atomic<uint64_t>                           m_nextId{1};
    std::atomic<uint64_t>                           m_totalConnections{0};
};
