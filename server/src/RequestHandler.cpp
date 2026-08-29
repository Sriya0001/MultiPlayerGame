#include "server/RequestHandler.h"
#include "server/Logger.h"
#include "server/Metrics.h"

// ── Construction ──────────────────────────────────────────────────────────────

RequestHandler::RequestHandler(std::shared_ptr<CacheManager> cache,
                               std::shared_ptr<DatabaseManager> db)
    : m_cache(cache)
    , m_db(db)
{
}

// ── Public interface ──────────────────────────────────────────────────────────

std::string RequestHandler::handle(const std::string& rawJson, uint64_t connId) {
    ScopedLatencyTimer timer; // records latency upon exiting handle()

    json request;
    try {
        request = json::parse(rawJson);
    } catch (const json::exception& ex) {
        Logger::warn("conn=" + std::to_string(connId)
                     + " JSON parse error: " + ex.what());
        MetricsRegistry::instance().incrementRequestsTotal("INVALID_JSON");
        MetricsRegistry::instance().incrementFailedRequests("INVALID_JSON");
        return errorResponse("invalid JSON").dump();
    }

    if (!request.contains("action") || !request["action"].is_string()) {
        Logger::warn("conn=" + std::to_string(connId) + " missing 'action' field");
        MetricsRegistry::instance().incrementRequestsTotal("MISSING_ACTION");
        MetricsRegistry::instance().incrementFailedRequests("MISSING_ACTION");
        return errorResponse("missing action").dump();
    }

    std::string action = request["action"].get<std::string>();
    Logger::info("conn=" + std::to_string(connId) + " action=" + action);
    MetricsRegistry::instance().incrementRequestsTotal(action);

    json response;
    try {
        if      (action == "JOIN")         response = handleJoin(request);
        else if (action == "MOVE")         response = handleMove(request);
        else if (action == "ATTACK")       response = handleAttack(request);
        else if (action == "CHAT")         response = handleChat(request);
        else if (action == "GET_STATE")    response = handleGetState(request);
        else if (action == "UPDATE_SCORE") response = handleUpdateScore(request);
        else if (action == "LEAVE")        response = handleLeave(request);
        else if (action == "GET_METRICS")  response = handleGetMetrics(request);
        else {
            Logger::warn("conn=" + std::to_string(connId)
                         + " unknown action: " + action);
            response = errorResponse("unknown action: " + action);
        }
    } catch (const std::exception& ex) {
        Logger::error("conn=" + std::to_string(connId)
                      + " handler exception: " + ex.what());
        response = errorResponse("internal server error");
    }

    if (response.value("status", "") == "ok") {
        MetricsRegistry::instance().incrementSuccessfulRequests(action);
    } else {
        MetricsRegistry::instance().incrementFailedRequests(action);
    }

    return response.dump();
}

// ── Action handlers ───────────────────────────────────────────────────────────

json RequestHandler::handleJoin(const json& req) {
    int player_id;
    std::string err;
    if (!requireInt(req, "player_id", player_id, err)) return errorResponse(err);

    if (!m_state.addPlayer(player_id, err)) return errorResponse(err);

    MetricsRegistry::instance().setActivePlayers(static_cast<int>(m_state.activePlayerCount()));

    if (m_cache && m_cache->isEnabled()) {
        json session = {{"player_id", player_id}, {"joined_at", std::time(nullptr)}};
        m_cache->setPlayerSession(player_id, session);
        m_cache->setPlayerPosition(player_id, 500, 500);
    }

    if (m_db) {
        m_db->upsertPlayer(player_id, "Player_" + std::to_string(player_id), 0, 0);
        m_db->recordEvent("match_1", player_id, "JOIN", "{}");
    }

    // Record event
    m_state.recordEvent(player_id, "JOIN", "{}");

    return json{
        {"status",    "ok"},
        {"action",    "JOIN"},
        {"player_id", player_id},
        {"x",         500},
        {"y",         500},
        {"health",    GameState::MAX_HEALTH},
        {"score",     0},
        {"message",   "Player " + std::to_string(player_id) + " joined the game"}
    };
}

json RequestHandler::handleMove(const json& req) {
    int player_id, x, y;
    std::string err;
    if (!requireInt(req, "player_id", player_id, err)) return errorResponse(err);
    if (!requireInt(req, "x",         x,         err)) return errorResponse(err);
    if (!requireInt(req, "y",         y,         err)) return errorResponse(err);

    int cx = std::clamp(x, GameState::MAP_MIN, GameState::MAP_MAX);
    int cy = std::clamp(y, GameState::MAP_MIN, GameState::MAP_MAX);

    if (!m_state.movePlayer(player_id, cx, cy, err)) return errorResponse(err);

    if (m_cache && m_cache->isEnabled()) {
        m_cache->setPlayerPosition(player_id, cx, cy);
    }

    m_state.recordEvent(player_id, "MOVE",
        "{\"x\":" + std::to_string(cx) + ",\"y\":" + std::to_string(cy) + "}");

    return json{
        {"status",    "ok"},
        {"action",    "MOVE"},
        {"player_id", player_id},
        {"x",         cx},
        {"y",         cy}
    };
}

json RequestHandler::handleAttack(const json& req) {
    int player_id, target_id;
    std::string err;
    if (!requireInt(req, "player_id", player_id, err)) return errorResponse(err);
    if (!requireInt(req, "target_id", target_id, err)) return errorResponse(err);

    AttackResult result;
    if (!m_state.attackPlayer(player_id, target_id, result, err))
        return errorResponse(err);

    m_state.recordEvent(player_id, "ATTACK",
        "{\"target_id\":" + std::to_string(target_id)
        + ",\"damage\":" + std::to_string(result.damage)
        + ",\"eliminated\":" + (result.targetEliminated ? "true" : "false") + "}");

    json resp = {
        {"status",         "ok"},
        {"action",         "ATTACK"},
        {"attacker_id",    player_id},
        {"target_id",      target_id},
        {"damage",         result.damage},
        {"target_health",  result.targetHealthAfter},
        {"attacker_score", result.attackerScore}
    };
    if (result.targetEliminated) resp["eliminated"] = true;
    return resp;
}

json RequestHandler::handleChat(const json& req) {
    int player_id;
    std::string message;
    std::string err;
    if (!requireInt   (req, "player_id", player_id, err)) return errorResponse(err);
    if (!requireString(req, "message",   message,   err)) return errorResponse(err);

    if (message.empty())
        return errorResponse("message cannot be empty");
    if (static_cast<int>(message.size()) > GameState::MAX_CHAT_LEN)
        return errorResponse("message too long (max "
                             + std::to_string(GameState::MAX_CHAT_LEN) + " chars)");
    if (!m_state.hasPlayer(player_id))
        return errorResponse("player_id " + std::to_string(player_id)
                             + " not in game");

    m_state.recordEvent(player_id, "CHAT", "{\"len\":" +
                        std::to_string(message.size()) + "}");

    return json{
        {"status",    "ok"},
        {"action",    "CHAT"},
        {"player_id", player_id},
        {"message",   message}
    };
}

json RequestHandler::handleGetState(const json& req) {
    if (req.contains("player_id") && req["player_id"].is_number_integer()) {
        int player_id = req["player_id"].get<int>();
        if (!m_state.hasPlayer(player_id))
            return errorResponse("player_id " + std::to_string(player_id) + " not in game");

        // If cache enabled, lookup our own cached session to verify cache hit
        if (m_cache && m_cache->isEnabled()) {
            json session;
            m_cache->getPlayerSession(player_id, session);
        }
    }

    json snap = m_state.snapshot();
    snap["status"] = "ok";
    snap["action"] = "GET_STATE";
    return snap;
}

json RequestHandler::handleUpdateScore(const json& req) {
    int player_id, score;
    std::string err;
    if (!requireInt(req, "player_id", player_id, err)) return errorResponse(err);
    if (!requireInt(req, "score",     score,     err)) return errorResponse(err);

    if (score < 0)
        return errorResponse("score cannot be negative");

    if (!m_state.setScore(player_id, score, err)) return errorResponse(err);

    m_state.recordEvent(player_id, "UPDATE_SCORE",
                        "{\"score\":" + std::to_string(score) + "}");

    return json{
        {"status",    "ok"},
        {"action",    "UPDATE_SCORE"},
        {"player_id", player_id},
        {"score",     score}
    };
}

json RequestHandler::handleLeave(const json& req) {
    int player_id;
    std::string err;
    if (!requireInt(req, "player_id", player_id, err)) return errorResponse(err);

    int finalScore = 0;
    if (!m_state.removePlayer(player_id, finalScore, err)) return errorResponse(err);

    MetricsRegistry::instance().setActivePlayers(static_cast<int>(m_state.activePlayerCount()));

    if (m_cache && m_cache->isEnabled()) {
        m_cache->deletePlayerSession(player_id);
    }

    if (m_db) {
        m_db->upsertPlayer(player_id, "Player_" + std::to_string(player_id), finalScore, 0);
        m_db->recordEvent("match_1", player_id, "LEAVE", "{\"final_score\":" + std::to_string(finalScore) + "}");
    }

    m_state.recordEvent(player_id, "LEAVE",
                        "{\"final_score\":" + std::to_string(finalScore) + "}");

    return json{
        {"status",      "ok"},
        {"action",      "LEAVE"},
        {"player_id",   player_id},
        {"final_score", finalScore},
        {"message",     "Player " + std::to_string(player_id) + " left the game"}
    };
}

json RequestHandler::handleGetMetrics(const json&) {
    auto m = MetricsRegistry::instance().toJson();
    m["status"] = "ok";
    m["action"] = "GET_METRICS";
    return m;
}

// ── Validation helpers ────────────────────────────────────────────────────────

bool RequestHandler::requireInt(const json& j, const std::string& key,
                                 int& out, std::string& err) {
    if (!j.contains(key)) {
        err = "missing required field: '" + key + "'";
        return false;
    }
    if (!j[key].is_number_integer()) {
        err = "field '" + key + "' must be an integer";
        return false;
    }
    out = j[key].get<int>();
    return true;
}

bool RequestHandler::requireString(const json& j, const std::string& key,
                                    std::string& out, std::string& err) {
    if (!j.contains(key)) {
        err = "missing required field: '" + key + "'";
        return false;
    }
    if (!j[key].is_string()) {
        err = "field '" + key + "' must be a string";
        return false;
    }
    out = j[key].get<std::string>();
    return true;
}

json RequestHandler::errorResponse(const std::string& message) {
    return json{{"status", "error"}, {"message", message}};
}
