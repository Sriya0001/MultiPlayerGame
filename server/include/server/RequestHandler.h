#pragma once

#include "server/GameState.h"
#include "server/CacheManager.h"
#include "server/DatabaseManager.h"

#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>
#include <memory>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// RequestHandler
// ─────────────────────────────────────────────────────────────────────────────
class RequestHandler {
public:
    explicit RequestHandler(std::shared_ptr<CacheManager> cache = nullptr,
                           std::shared_ptr<DatabaseManager> db = nullptr);

    RequestHandler(const RequestHandler&)            = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    void setCacheManager(std::shared_ptr<CacheManager> cache) { m_cache = cache; }
    void setDatabaseManager(std::shared_ptr<DatabaseManager> db) { m_db = db; }

    std::string handle(const std::string& rawJson, uint64_t connId);

    // Expose GameState for metrics / tests
    const GameState& gameState() const { return m_state; }
    GameState&       gameState()       { return m_state; }

private:
    json handleJoin        (const json& req);
    json handleMove        (const json& req);
    json handleAttack      (const json& req);
    json handleChat        (const json& req);
    json handleGetState    (const json& req);
    json handleUpdateScore (const json& req);
    json handleLeave       (const json& req);
    json handleGetMetrics  (const json& req);

    static bool requireInt   (const json& j, const std::string& key,
                               int& out, std::string& err);
    static bool requireString(const json& j, const std::string& key,
                               std::string& out, std::string& err);
    static json errorResponse(const std::string& message);

    GameState m_state;
    std::shared_ptr<CacheManager> m_cache;
    std::shared_ptr<DatabaseManager> m_db;
};
