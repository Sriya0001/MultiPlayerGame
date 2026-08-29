#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

// Forward declaration
typedef struct MYSQL MYSQL;
typedef struct MYSQL_STMT MYSQL_STMT;

struct PlayerDbRecord {
    int         player_id{0};
    std::string username;
    int         score{0};
    int         high_score{0};
    int         kills{0};
    std::string created_at;
    std::string updated_at;
};

struct TestRunDbRecord {
    std::string run_id;
    std::string test_name;
    int         concurrent_users{0};
    int         duration_seconds{0};
    int         total_requests{0};
    int         successful_requests{0};
    int         failed_requests{0};
    double      requests_per_second{0.0};
    double      avg_latency_ms{0.0};
    double      p50_latency_ms{0.0};
    double      p95_latency_ms{0.0};
    double      p99_latency_ms{0.0};
    double      max_latency_ms{0.0};
    double      error_rate_percent{0.0};
};

class DatabaseManager {
public:
    DatabaseManager(const std::string& host = "127.0.0.1",
                    int port = 3306,
                    const std::string& user = "game_user",
                    const std::string& password = "game_pass",
                    const std::string& database = "game_server");
    ~DatabaseManager();

    bool open();
    void close();
    bool isConnected() const { return m_connected; }

    bool initSchema(const std::string& schemaSqlPath = "database/schema_mysql.sql");

    // ── CRUD Operations (Parameterized MySQL Statements) ───────────────────────
    bool upsertPlayer(int playerId, const std::string& username, int score, int kills);
    std::optional<PlayerDbRecord> getPlayer(int playerId);

    bool recordEvent(const std::string& matchId, int playerId,
                     const std::string& eventType, const std::string& eventData);

    bool saveMatch(const std::string& matchId, const std::string& status,
                   int totalPlayers, int totalKills, int winnerPlayerId);

    bool saveTestRun(const TestRunDbRecord& run);

    // Diagnostics
    int64_t getPlayerCount();
    int64_t getEventCount();

private:
    std::string m_host;
    int         m_port;
    std::string m_user;
    std::string m_password;
    std::string m_database;

    MYSQL*      m_conn{nullptr};
    bool        m_connected{false};
    mutable std::mutex m_mutex;
};
