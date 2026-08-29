#include "server/GameState.h"
#include "server/Logger.h"

#include <algorithm>
#include <cmath>

// ── Construction ──────────────────────────────────────────────────────────────

GameState::GameState()
    : m_matchStart(std::chrono::steady_clock::now())
{}

// ── Player operations ─────────────────────────────────────────────────────────

bool GameState::addPlayer(int player_id, std::string& err) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_players.count(player_id)) {
        err = "player_id " + std::to_string(player_id) + " already in game";
        return false;
    }

    PlayerState p;
    p.player_id = player_id;
    p.joinedAt  = std::chrono::steady_clock::now();
    m_players.emplace(player_id, p);

    // Transition WAITING → ACTIVE on first join
    if (m_status == MatchStatus::WAITING) {
        m_status     = MatchStatus::ACTIVE;
        m_matchStart = std::chrono::steady_clock::now();
        Logger::info("Match started (first player joined).");
    }

    Logger::info("Player joined: id=" + std::to_string(player_id)
                 + " total_active=" + std::to_string(m_players.size()));
    return true;
}

bool GameState::removePlayer(int player_id, int& finalScore, std::string& err) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_players.find(player_id);
    if (it == m_players.end()) {
        err = "player_id " + std::to_string(player_id) + " not in game";
        return false;
    }

    finalScore = it->second.score;
    m_players.erase(it);

    Logger::info("Player left: id=" + std::to_string(player_id)
                 + " final_score=" + std::to_string(finalScore)
                 + " remaining=" + std::to_string(m_players.size()));

    // Transition ACTIVE → FINISHED when last player leaves
    if (m_status == MatchStatus::ACTIVE && m_players.empty()) {
        m_status = MatchStatus::FINISHED;
        Logger::info("Match finished (no players remaining).");
    }

    return true;
}

bool GameState::movePlayer(int player_id, int x, int y, std::string& err) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_players.find(player_id);
    if (it == m_players.end()) {
        err = "player_id " + std::to_string(player_id) + " not in game";
        return false;
    }

    it->second.x = std::clamp(x, MAP_MIN, MAP_MAX);
    it->second.y = std::clamp(y, MAP_MIN, MAP_MAX);
    return true;
}

bool GameState::attackPlayer(int attacker_id, int target_id,
                              AttackResult& result, std::string& err) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (attacker_id == target_id) {
        err = "cannot attack yourself";
        return false;
    }

    auto attIt = m_players.find(attacker_id);
    if (attIt == m_players.end()) {
        err = "attacker " + std::to_string(attacker_id) + " not in game";
        return false;
    }

    auto tgtIt = m_players.find(target_id);
    if (tgtIt == m_players.end()) {
        err = "target " + std::to_string(target_id) + " not in game";
        return false;
    }

    result.damage           = ATTACK_DAMAGE;
    tgtIt->second.health    = std::max(0, tgtIt->second.health - ATTACK_DAMAGE);
    result.targetHealthAfter = tgtIt->second.health;
    result.targetEliminated  = (result.targetHealthAfter == 0);

    // Award score
    int pts = result.targetEliminated ? 100 : 10;
    attIt->second.score += pts;
    result.attackerScore  = attIt->second.score;

    if (result.targetEliminated) {
        ++attIt->second.kills;
        ++m_totalKills;
        Logger::info("Player eliminated: id=" + std::to_string(target_id)
                     + " by=" + std::to_string(attacker_id));
        m_players.erase(tgtIt);

        if (m_status == MatchStatus::ACTIVE && m_players.empty()) {
            m_status = MatchStatus::FINISHED;
            Logger::info("Match finished (last player eliminated).");
        }
    }

    return true;
}

bool GameState::setScore(int player_id, int score, std::string& err) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_players.find(player_id);
    if (it == m_players.end()) {
        err = "player_id " + std::to_string(player_id) + " not in game";
        return false;
    }

    it->second.score = score;
    return true;
}

// ── Queries ───────────────────────────────────────────────────────────────────

bool GameState::hasPlayer(int player_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_players.count(player_id) > 0;
}

json GameState::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Build players array
    json players_arr = json::array();
    for (const auto& [id, p] : m_players) {
        auto age = std::chrono::steady_clock::now() - p.joinedAt;
        players_arr.push_back({
            {"player_id", p.player_id},
            {"x",         p.x},
            {"y",         p.y},
            {"health",    p.health},
            {"score",     p.score},
            {"kills",     p.kills},
            {"session_s", std::chrono::duration<double>(age).count()}
        });
    }

    // Build recent events (last 5)
    json events_arr = json::array();
    std::size_t evStart = m_events.size() > 5 ? m_events.size() - 5 : 0;
    for (std::size_t i = evStart; i < m_events.size(); ++i) {
        const auto& ev = m_events[i];
        events_arr.push_back({
            {"event_id",   ev.event_id},
            {"player_id",  ev.player_id},
            {"event_type", ev.event_type},
            {"data",       ev.data}
        });
    }

    // Elapsed time
    auto elapsed = std::chrono::steady_clock::now() - m_matchStart;
    double elapsedSec = std::chrono::duration<double>(elapsed).count();

    return json{
        {"match_status",    matchStatusString()},
        {"elapsed_seconds", elapsedSec},
        {"player_count",    static_cast<int>(m_players.size())},
        {"total_kills",     m_totalKills},
        {"total_events",    m_nextEventId.load() - 1},
        {"players",         players_arr},
        {"recent_events",   events_arr}
    };
}

std::size_t GameState::activePlayerCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_players.size();
}

MatchStatus GameState::status() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

double GameState::elapsedSeconds() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto elapsed = std::chrono::steady_clock::now() - m_matchStart;
    return std::chrono::duration<double>(elapsed).count();
}

uint64_t GameState::totalEvents() const {
    return m_nextEventId.load() - 1;
}

uint64_t GameState::totalKills() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalKills;
}

// ── Event log ─────────────────────────────────────────────────────────────────

void GameState::recordEvent(int player_id, const std::string& type,
                            const std::string& data) {
    GameEvent ev;
    ev.event_id   = m_nextEventId.fetch_add(1, std::memory_order_relaxed);
    ev.player_id  = player_id;
    ev.event_type = type;
    ev.data       = data;
    ev.timestamp  = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.push_back(std::move(ev));

    // Cap at MAX_EVENTS (ring-buffer behaviour)
    if (m_events.size() > MAX_EVENTS) {
        m_events.pop_front();
    }
}

std::vector<GameEvent> GameState::recentEvents(std::size_t n) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t start = m_events.size() > n ? m_events.size() - n : 0;
    return std::vector<GameEvent>(m_events.begin() + static_cast<std::ptrdiff_t>(start),
                                  m_events.end());
}

// ── Helpers ───────────────────────────────────────────────────────────────────

std::string GameState::matchStatusString() const {
    switch (m_status) {
        case MatchStatus::WAITING:  return "WAITING";
        case MatchStatus::ACTIVE:   return "ACTIVE";
        case MatchStatus::FINISHED: return "FINISHED";
        default:                    return "UNKNOWN";
    }
}
