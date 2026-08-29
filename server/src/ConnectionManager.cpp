#include "server/ConnectionManager.h"
#include "server/Logger.h"

uint64_t ConnectionManager::registerConnection(int fd, const std::string& remoteAddr) {
    uint64_t id = m_nextId.fetch_add(1, std::memory_order_relaxed);

    ConnectionInfo info;
    info.connectionId = id;
    info.fd           = fd;
    info.remoteAddr   = remoteAddr;
    info.connectedAt  = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.emplace(id, std::move(info));
    }

    m_totalConnections.fetch_add(1, std::memory_order_relaxed);

    Logger::debug("Connection registered: id=" + std::to_string(id)
                  + " addr=" + remoteAddr
                  + " active=" + std::to_string(activeCount()));
    return id;
}

void ConnectionManager::unregisterConnection(uint64_t connectionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(connectionId);
    if (it == m_connections.end()) return;

    auto elapsed = std::chrono::steady_clock::now() - it->second.connectedAt;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    Logger::debug("Connection closed:     id=" + std::to_string(connectionId)
                  + " addr=" + it->second.remoteAddr
                  + " msgs_rx=" + std::to_string(it->second.messagesReceived)
                  + " msgs_tx=" + std::to_string(it->second.messagesSent)
                  + " duration=" + std::to_string(ms) + "ms");

    m_connections.erase(it);
}

void ConnectionManager::recordMessageReceived(uint64_t connectionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(connectionId);
    if (it != m_connections.end()) ++it->second.messagesReceived;
}

void ConnectionManager::recordMessageSent(uint64_t connectionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_connections.find(connectionId);
    if (it != m_connections.end()) ++it->second.messagesSent;
}

std::size_t ConnectionManager::activeCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connections.size();
}

uint64_t ConnectionManager::totalConnections() const {
    return m_totalConnections.load(std::memory_order_relaxed);
}
