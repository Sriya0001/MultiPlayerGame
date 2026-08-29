#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <chrono>
#include <atomic>
#include <cstdint>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// PlayerState
// ─────────────────────────────────────────────────────────────────────────────
struct PlayerState {
    int  player_id;
    int  x{500};
    int  y{500};
    int  health{100};
    int  score{0};
    int  kills{0};
    std::chrono::steady_clock::time_point joinedAt;
};

// ─────────────────────────────────────────────────────────────────────────────
// GameEvent — recorded every time a meaningful action occurs
// (written to MySQL in Milestone 10; stored in memory ring-buffer for now)
// ─────────────────────────────────────────────────────────────────────────────
struct GameEvent {
    uint64_t    event_id;
    int         player_id;
    std::string event_type;   // "JOIN", "MOVE", "ATTACK", "CHAT", "LEAVE", etc.
    std::string data;         // JSON-encoded extra context
    std::chrono::system_clock::time_point timestamp;
};

// ─────────────────────────────────────────────────────────────────────────────
// MatchStatus
// ─────────────────────────────────────────────────────────────────────────────
enum class MatchStatus { WAITING, ACTIVE, FINISHED };

// ─────────────────────────────────────────────────────────────────────────────
// AttackResult — returned by GameState::attackPlayer
// ─────────────────────────────────────────────────────────────────────────────
struct AttackResult {
    int  damage{0};
    int  targetHealthAfter{0};
    int  attackerScore{0};
    bool targetEliminated{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// GameState
//
// Single authoritative source of truth for all game data.
// Thread-safe — all public methods acquire m_mutex.
//
// Design decisions:
//   - One mutex for the whole state. At Milestone 5 we measure whether this
//     becomes a bottleneck under the thread pool. If it does, we can split
//     into reader-writer lock in a future milestone.
//   - Events stored in a capped deque (ring-buffer semantics). Overflow events
//     are discarded from the front to keep memory bounded.
//   - MySQL persistence added in Milestone 10.
// ─────────────────────────────────────────────────────────────────────────────
class GameState {
public:
    static constexpr int MAP_MIN        = 0;
    static constexpr int MAP_MAX        = 1000;
    static constexpr int MAX_HEALTH     = 100;
    static constexpr int ATTACK_DAMAGE  = 25;
    static constexpr int MAX_CHAT_LEN   = 256;
    static constexpr std::size_t MAX_EVENTS = 1000; // ring-buffer cap

    GameState();

    // Non-copyable
    GameState(const GameState&)            = delete;
    GameState& operator=(const GameState&) = delete;

    // ── Player operations ─────────────────────────────────────────────────────

    // Returns false + sets err if player already in game.
    bool addPlayer   (int player_id, std::string& err);

    // Returns false + sets err if player not in game.
    // On success, sets finalScore and removes the player.
    bool removePlayer(int player_id, int& finalScore, std::string& err);

    // Move player to (x,y). Clamps to map bounds silently.
    bool movePlayer  (int player_id, int x, int y, std::string& err);

    // Attack target. Fills AttackResult. Removes target if eliminated.
    bool attackPlayer(int attacker_id, int target_id,
                      AttackResult& result, std::string& err);

    // Overwrite player score directly.
    bool setScore    (int player_id, int score, std::string& err);

    // ── Queries ───────────────────────────────────────────────────────────────

    bool hasPlayer(int player_id) const;

    // Full snapshot as JSON (for GET_STATE response).
    json snapshot() const;

    // Lightweight stats (for metrics endpoint).
    std::size_t  activePlayerCount() const;
    MatchStatus  status()            const;
    double       elapsedSeconds()    const;
    uint64_t     totalEvents()       const;
    uint64_t     totalKills()        const;

    // ── Event log ─────────────────────────────────────────────────────────────

    // Record an event. Called by RequestHandler after each successful action.
    void recordEvent(int player_id, const std::string& type,
                     const std::string& data = "");

    // Return up to n most-recent events (newest last).
    std::vector<GameEvent> recentEvents(std::size_t n = 10) const;

private:
    std::string matchStatusString() const;

    mutable std::mutex                       m_mutex;
    std::unordered_map<int, PlayerState>     m_players;
    std::deque<GameEvent>                    m_events;

    MatchStatus                              m_status{MatchStatus::WAITING};
    std::chrono::steady_clock::time_point    m_matchStart;
    std::atomic<uint64_t>                    m_nextEventId{1};
    uint64_t                                 m_totalKills{0};
};
