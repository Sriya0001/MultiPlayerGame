#include "simulator/PlayerBot.h"
#include "server/MessageFramer.h"
#include <nlohmann/json.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <iostream>

using json = nlohmann::json;

PlayerBot::PlayerBot(int playerId, const SimulatorConfig& config, MetricsCollector& collector)
    : m_playerId(playerId)
    , m_config(config)
    , m_collector(collector)
    , m_socketFd(-1)
    , m_rng(config.seed + playerId)
{
}

PlayerBot::~PlayerBot() {
    disconnectSocket();
}

bool PlayerBot::connectSocket() {
    m_socketFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFd < 0) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_config.port);
    if (::inet_pton(AF_INET, m_config.host.c_str(), &addr.sin_addr) <= 0) {
        ::close(m_socketFd);
        m_socketFd = -1;
        return false;
    }

    if (::connect(m_socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(m_socketFd);
        m_socketFd = -1;
        return false;
    }

    return true;
}

void PlayerBot::disconnectSocket() {
    if (m_socketFd >= 0) {
        ::close(m_socketFd);
        m_socketFd = -1;
    }
}

bool PlayerBot::sendAndReceive(const std::string& action, const std::string& jsonPayload) {
    if (m_socketFd < 0) return false;

    auto t0 = std::chrono::high_resolution_clock::now();

    auto frame = MessageFramer::encode(jsonPayload);
    if (!MessageFramer::writeAll(m_socketFd, frame)) {
        m_localMetrics.push_back({action, 0, false});
        return false;
    }

    uint8_t lenBuf[MessageFramer::HEADER_SIZE];
    if (!MessageFramer::readExact(m_socketFd, lenBuf, sizeof(lenBuf))) {
        m_localMetrics.push_back({action, 0, false});
        return false;
    }

    uint32_t payloadLen = static_cast<uint32_t>(lenBuf[0])
                        | (static_cast<uint32_t>(lenBuf[1]) << 8)
                        | (static_cast<uint32_t>(lenBuf[2]) << 16)
                        | (static_cast<uint32_t>(lenBuf[3]) << 24);

    if (payloadLen > MessageFramer::MAX_MESSAGE_SIZE) {
        m_localMetrics.push_back({action, 0, false});
        return false;
    }

    std::string responsePayload(payloadLen, '\0');
    if (!MessageFramer::readExact(m_socketFd, responsePayload.data(), payloadLen)) {
        m_localMetrics.push_back({action, 0, false});
        return false;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    uint64_t latencyMicros = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    bool success = false;
    try {
        auto resp = json::parse(responsePayload);
        if (resp.value("status", "") == "ok") {
            success = true;
        }
    } catch (...) {
        success = false;
    }

    m_localMetrics.push_back({action, latencyMicros, success});
    return success;
}

void PlayerBot::run(const std::atomic<bool>& stopFlag) {
    m_localMetrics.reserve(200);

    if (!connectSocket()) {
        m_collector.record("CONNECT", 0, false);
        return;
    }

    // 1. JOIN
    json joinReq = {{"action", "JOIN"}, {"player_id", m_playerId}};
    sendAndReceive("JOIN", joinReq.dump());

    std::uniform_int_distribution<int> actionDist(1, 100);
    std::uniform_int_distribution<int> posDist(0, 1000);
    std::uniform_int_distribution<int> targetDist(m_config.basePlayerId, m_config.basePlayerId + m_config.playerCount - 1);

    auto startTime = std::chrono::steady_clock::now();

    // 2. Action loop
    while (!stopFlag.load()) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count() >= m_config.durationSeconds) {
            break;
        }

        int roll = actionDist(m_rng);
        if (roll <= 60) {
            // MOVE (60%)
            int x = posDist(m_rng);
            int y = posDist(m_rng);
            json moveReq = {{"action", "MOVE"}, {"player_id", m_playerId}, {"x", x}, {"y", y}};
            sendAndReceive("MOVE", moveReq.dump());
        } else if (roll <= 75) {
            // ATTACK (15%) - attack nearest neighbour
            int targetId = (m_playerId == m_config.basePlayerId + m_config.playerCount - 1)
                           ? m_config.basePlayerId
                           : m_playerId + 1;
            json attackReq = {{"action", "ATTACK"}, {"player_id", m_playerId}, {"target_id", targetId}};
            sendAndReceive("ATTACK", attackReq.dump());
        } else if (roll <= 90) {
            // GET_STATE (15%)
            json stateReq = {{"action", "GET_STATE"}, {"player_id", m_playerId}};
            sendAndReceive("GET_STATE", stateReq.dump());
        } else {
            // CHAT (10%)
            json chatReq = {{"action", "CHAT"}, {"player_id", m_playerId}, {"message", "ggwp"}};
            sendAndReceive("CHAT", chatReq.dump());
        }

        if (m_config.actionIntervalMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_config.actionIntervalMs));
        }
    }

    // 3. LEAVE
    json leaveReq = {{"action", "LEAVE"}, {"player_id", m_playerId}};
    sendAndReceive("LEAVE", leaveReq.dump());

    disconnectSocket();

    // Flush metrics to global collector
    m_collector.recordBatch(m_localMetrics);
}
