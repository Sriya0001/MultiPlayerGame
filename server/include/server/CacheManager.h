#pragma once

#include "server/RedisClient.h"
#include "server/Metrics.h"
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// CacheManager
//
// Manages caching for active player sessions, fast state lookups, and leaderboards.
// Automatically falls back to in-memory state on cache miss or when Redis is disabled.
// ─────────────────────────────────────────────────────────────────────────────

class CacheManager {
public:
    CacheManager(bool enabled = false, const std::string& redisHost = "127.0.0.1", int redisPort = 6379);
    ~CacheManager() = default;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    bool ping();

    // Session cache (key: "session:player:<id>")
    bool setPlayerSession(int playerId, const nlohmann::json& sessionData, int ttlSec = 300);
    bool getPlayerSession(int playerId, nlohmann::json& outSessionData);
    bool deletePlayerSession(int playerId);

    // Player position cache (key: "pos:player:<id>")
    bool setPlayerPosition(int playerId, int x, int y);
    bool getPlayerPosition(int playerId, int& outX, int& outY);

private:
    bool m_enabled;
    std::unique_ptr<RedisPool> m_pool;
};
