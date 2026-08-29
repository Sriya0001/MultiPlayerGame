#include "server/CacheManager.h"
#include "server/Logger.h"

CacheManager::CacheManager(bool enabled, const std::string& redisHost, int redisPort)
    : m_enabled(enabled)
{
    if (m_enabled) {
        m_pool = std::make_unique<RedisPool>(redisHost, redisPort, 64);
        Logger::info("CacheManager initialized with Redis at " + redisHost + ":" + std::to_string(redisPort));
    } else {
        Logger::info("CacheManager initialized in direct in-memory fallback mode (Redis disabled)");
    }
}

bool CacheManager::ping() {
    if (!m_enabled || !m_pool) return false;
    return m_pool->isAvailable();
}

bool CacheManager::setPlayerSession(int playerId, const nlohmann::json& sessionData, int ttlSec) {
    if (!m_enabled || !m_pool) return false;
    std::string key = "session:player:" + std::to_string(playerId);
    auto conn = m_pool->acquire();
    if (!conn) return false;
    return conn->get()->set(key, sessionData.dump(), ttlSec);
}

bool CacheManager::getPlayerSession(int playerId, nlohmann::json& outSessionData) {
    if (!m_enabled || !m_pool) {
        MetricsRegistry::instance().incrementRedisCacheMisses();
        return false;
    }

    std::string key = "session:player:" + std::to_string(playerId);
    auto conn = m_pool->acquire();
    if (!conn) {
        MetricsRegistry::instance().incrementRedisCacheMisses();
        return false;
    }

    std::string rawJson;
    if (conn->get()->get(key, rawJson)) {
        try {
            outSessionData = nlohmann::json::parse(rawJson);
            MetricsRegistry::instance().incrementRedisCacheHits();
            return true;
        } catch (...) {
            MetricsRegistry::instance().incrementRedisCacheMisses();
            return false;
        }
    }

    MetricsRegistry::instance().incrementRedisCacheMisses();
    return false;
}

bool CacheManager::deletePlayerSession(int playerId) {
    if (!m_enabled || !m_pool) return false;
    std::string key = "session:player:" + std::to_string(playerId);
    auto conn = m_pool->acquire();
    if (!conn) return false;
    return conn->get()->del(key);
}

bool CacheManager::setPlayerPosition(int playerId, int x, int y) {
    if (!m_enabled || !m_pool) return false;
    std::string key = "pos:player:" + std::to_string(playerId);
    std::string val = std::to_string(x) + "," + std::to_string(y);
    auto conn = m_pool->acquire();
    if (!conn) return false;
    return conn->get()->set(key, val, 60);
}

bool CacheManager::getPlayerPosition(int playerId, int& outX, int& outY) {
    if (!m_enabled || !m_pool) {
        MetricsRegistry::instance().incrementRedisCacheMisses();
        return false;
    }

    std::string key = "pos:player:" + std::to_string(playerId);
    auto conn = m_pool->acquire();
    if (!conn) {
        MetricsRegistry::instance().incrementRedisCacheMisses();
        return false;
    }

    std::string val;
    if (conn->get()->get(key, val)) {
        auto comma = val.find(',');
        if (comma != std::string::npos) {
            outX = std::stoi(val.substr(0, comma));
            outY = std::stoi(val.substr(comma + 1));
            MetricsRegistry::instance().incrementRedisCacheHits();
            return true;
        }
    }

    MetricsRegistry::instance().incrementRedisCacheMisses();
    return false;
}
