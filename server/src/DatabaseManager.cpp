#include "server/DatabaseManager.h"
#include "server/Logger.h"
#include "server/Metrics.h"

#include <mysql/mysql.h>
#include <fstream>
#include <sstream>
#include <cstring>

DatabaseManager::DatabaseManager(const std::string& host, int port,
                                 const std::string& user, const std::string& password,
                                 const std::string& database)
    : m_host(host), m_port(port), m_user(user), m_password(password), m_database(database)
{
}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::open() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_connected && m_conn) {
        if (mysql_ping(m_conn) == 0) {
            return true;
        }
    }

    if (m_conn) {
        mysql_close(m_conn);
        m_conn = nullptr;
    }

    m_conn = mysql_init(nullptr);
    if (!m_conn) {
        Logger::error("MySQL init failed.");
        m_connected = false;
        return false;
    }

    // Set connection timeout
    unsigned int timeoutSec = 3;
    mysql_options(m_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeoutSec);
    bool reconnect = true;
    mysql_options(m_conn, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(m_conn, m_host.c_str(), m_user.c_str(), m_password.c_str(),
                            m_database.c_str(), m_port, nullptr, 0)) {
        Logger::error("MySQL connect failed to " + m_host + ":" + std::to_string(m_port) + " -> " + mysql_error(m_conn));
        mysql_close(m_conn);
        m_conn = nullptr;
        m_connected = false;
        return false;
    }

    m_connected = true;
    Logger::info("DatabaseManager connected to MySQL 8.0 at " + m_host + ":" + std::to_string(m_port) + " (DB: " + m_database + ")");
    return true;
}

void DatabaseManager::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_conn) {
        mysql_close(m_conn);
        m_conn = nullptr;
    }
    m_connected = false;
}

bool DatabaseManager::initSchema(const std::string& schemaSqlPath) {
    if (!open()) return false;

    std::ifstream ifs(schemaSqlPath);
    if (!ifs.is_open()) {
        Logger::warn("MySQL schema file not found at " + schemaSqlPath + ", skipping auto-init (tables should exist).");
        return true;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string sql = ss.str();

    std::lock_guard<std::mutex> lock(m_mutex);
    // Execute schema statements
    std::istringstream stream(sql);
    std::string statement;
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("--", 0) == 0) continue; // skip comments
        statement += line + "\n";
        if (line.find(';') != std::string::npos) {
            if (mysql_query(m_conn, statement.c_str()) != 0) {
                Logger::warn("MySQL schema statement warning: " + std::string(mysql_error(m_conn)));
            }
            statement.clear();
        }
    }

    Logger::info("MySQL schema verified successfully.");
    return true;
}

bool DatabaseManager::upsertPlayer(int playerId, const std::string& username, int score, int kills) {
    if (!open()) return false;

    auto t0 = std::chrono::high_resolution_clock::now();

    const char* sql = R"(
        INSERT INTO players (player_id, username, score, high_score, kills, updated_at)
        VALUES (?, ?, ?, ?, ?, NOW())
        ON DUPLICATE KEY UPDATE
            score = VALUES(score),
            high_score = GREATEST(high_score, VALUES(score)),
            kills = GREATEST(kills, VALUES(kills)),
            updated_at = NOW();
    )";

    std::lock_guard<std::mutex> lock(m_mutex);
    MYSQL_STMT* stmt = mysql_stmt_init(m_conn);
    if (!stmt) {
        MetricsRegistry::instance().recordDatabaseOperation(0, false);
        return false;
    }

    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        Logger::error("MySQL stmt prepare error: " + std::string(mysql_stmt_error(stmt)));
        mysql_stmt_close(stmt);
        MetricsRegistry::instance().recordDatabaseOperation(0, false);
        return false;
    }

    MYSQL_BIND bind[5];
    std::memset(bind, 0, sizeof(bind));

    // 1. player_id
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &playerId;

    // 2. username
    unsigned long userLen = static_cast<unsigned long>(username.size());
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = const_cast<char*>(username.data());
    bind[1].length = &userLen;

    // 3. score
    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &score;

    // 4. high_score
    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &score;

    // 5. kills
    bind[4].buffer_type = MYSQL_TYPE_LONG;
    bind[4].buffer = &kills;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        MetricsRegistry::instance().recordDatabaseOperation(0, false);
        return false;
    }

    int rc = mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);

    auto t1 = std::chrono::high_resolution_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();
    bool ok = (rc == 0);
    MetricsRegistry::instance().recordDatabaseOperation(sec, ok);
    return ok;
}

std::optional<PlayerDbRecord> DatabaseManager::getPlayer(int playerId) {
    if (!open()) return std::nullopt;

    auto t0 = std::chrono::high_resolution_clock::now();
    const char* sql = "SELECT player_id, username, score, high_score, kills, created_at, updated_at FROM players WHERE player_id = ?;";

    std::lock_guard<std::mutex> lock(m_mutex);
    MYSQL_STMT* stmt = mysql_stmt_init(m_conn);
    if (!stmt) return std::nullopt;

    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        MetricsRegistry::instance().recordDatabaseOperation(0, false);
        return std::nullopt;
    }

    MYSQL_BIND paramBind[1];
    std::memset(paramBind, 0, sizeof(paramBind));
    paramBind[0].buffer_type = MYSQL_TYPE_LONG;
    paramBind[0].buffer = &playerId;
    mysql_stmt_bind_param(stmt, paramBind);

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        MetricsRegistry::instance().recordDatabaseOperation(0, false);
        return std::nullopt;
    }

    int outId = 0, outScore = 0, outHighScore = 0, outKills = 0;
    char outUser[65] = {0};
    char outCreatedAt[64] = {0};
    char outUpdatedAt[64] = {0};
    unsigned long lenUser = 0, lenCreated = 0, lenUpdated = 0;

    MYSQL_BIND resBind[7];
    std::memset(resBind, 0, sizeof(resBind));

    resBind[0].buffer_type = MYSQL_TYPE_LONG;
    resBind[0].buffer = &outId;

    resBind[1].buffer_type = MYSQL_TYPE_STRING;
    resBind[1].buffer = outUser;
    resBind[1].buffer_length = sizeof(outUser) - 1;
    resBind[1].length = &lenUser;

    resBind[2].buffer_type = MYSQL_TYPE_LONG;
    resBind[2].buffer = &outScore;

    resBind[3].buffer_type = MYSQL_TYPE_LONG;
    resBind[3].buffer = &outHighScore;

    resBind[4].buffer_type = MYSQL_TYPE_LONG;
    resBind[4].buffer = &outKills;

    resBind[5].buffer_type = MYSQL_TYPE_STRING;
    resBind[5].buffer = outCreatedAt;
    resBind[5].buffer_length = sizeof(outCreatedAt) - 1;
    resBind[5].length = &lenCreated;

    resBind[6].buffer_type = MYSQL_TYPE_STRING;
    resBind[6].buffer = outUpdatedAt;
    resBind[6].buffer_length = sizeof(outUpdatedAt) - 1;
    resBind[6].length = &lenUpdated;

    mysql_stmt_bind_result(stmt, resBind);

    std::optional<PlayerDbRecord> result;
    if (mysql_stmt_fetch(stmt) == 0) {
        PlayerDbRecord rec;
        rec.player_id = outId;
        rec.username = std::string(outUser, lenUser);
        rec.score = outScore;
        rec.high_score = outHighScore;
        rec.kills = outKills;
        rec.created_at = std::string(outCreatedAt, lenCreated);
        rec.updated_at = std::string(outUpdatedAt, lenUpdated);
        result = rec;
    }

    mysql_stmt_close(stmt);
    auto t1 = std::chrono::high_resolution_clock::now();
    MetricsRegistry::instance().recordDatabaseOperation(std::chrono::duration<double>(t1 - t0).count(), true);
    return result;
}

bool DatabaseManager::recordEvent(const std::string& matchId, int playerId,
                                  const std::string& eventType, const std::string& eventData) {
    if (!open()) return false;

    auto t0 = std::chrono::high_resolution_clock::now();
    const char* sql = "INSERT INTO game_events (match_id, player_id, event_type, event_data, created_at) VALUES (?, ?, ?, ?, NOW());";

    std::lock_guard<std::mutex> lock(m_mutex);
    MYSQL_STMT* stmt = mysql_stmt_init(m_conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        MetricsRegistry::instance().recordDatabaseOperation(0, false);
        return false;
    }

    unsigned long mLen = static_cast<unsigned long>(matchId.size());
    unsigned long tLen = static_cast<unsigned long>(eventType.size());
    unsigned long dLen = static_cast<unsigned long>(eventData.size());

    MYSQL_BIND bind[4];
    std::memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = const_cast<char*>(matchId.data());
    bind[0].length = &mLen;

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = &playerId;

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = const_cast<char*>(eventType.data());
    bind[2].length = &tLen;

    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = const_cast<char*>(eventData.data());
    bind[3].length = &dLen;

    mysql_stmt_bind_param(stmt, bind);
    int rc = mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);

    auto t1 = std::chrono::high_resolution_clock::now();
    bool ok = (rc == 0);
    MetricsRegistry::instance().recordDatabaseOperation(std::chrono::duration<double>(t1 - t0).count(), ok);
    return ok;
}

bool DatabaseManager::saveMatch(const std::string& matchId, const std::string& status,
                                int totalPlayers, int totalKills, int winnerPlayerId) {
    if (!open()) return false;

    auto t0 = std::chrono::high_resolution_clock::now();
    const char* sql = R"(
        INSERT INTO matches (match_id, match_status, end_time, total_players, total_kills, winner_player_id)
        VALUES (?, ?, NOW(), ?, ?, ?)
        ON DUPLICATE KEY UPDATE
            match_status = VALUES(match_status),
            end_time = NOW(),
            total_players = VALUES(total_players),
            total_kills = VALUES(total_kills),
            winner_player_id = VALUES(winner_player_id);
    )";

    std::lock_guard<std::mutex> lock(m_mutex);
    MYSQL_STMT* stmt = mysql_stmt_init(m_conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        MetricsRegistry::instance().recordDatabaseOperation(0, false);
        return false;
    }

    unsigned long mLen = static_cast<unsigned long>(matchId.size());
    unsigned long sLen = static_cast<unsigned long>(status.size());

    MYSQL_BIND bind[5];
    std::memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = const_cast<char*>(matchId.data());
    bind[0].length = &mLen;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = const_cast<char*>(status.data());
    bind[1].length = &sLen;

    bind[2].buffer_type = MYSQL_TYPE_LONG;
    bind[2].buffer = &totalPlayers;

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &totalKills;

    bind[4].buffer_type = MYSQL_TYPE_LONG;
    bind[4].buffer = &winnerPlayerId;

    mysql_stmt_bind_param(stmt, bind);
    int rc = mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);

    auto t1 = std::chrono::high_resolution_clock::now();
    bool ok = (rc == 0);
    MetricsRegistry::instance().recordDatabaseOperation(std::chrono::duration<double>(t1 - t0).count(), ok);
    return ok;
}

bool DatabaseManager::saveTestRun(const TestRunDbRecord& run) {
    if (!open()) return false;

    const char* sql = R"(
        INSERT INTO test_runs (
            run_id, test_name, concurrent_users, duration_seconds,
            total_requests, successful_requests, failed_requests,
            requests_per_second, avg_latency_ms, p50_latency_ms,
            p95_latency_ms, p99_latency_ms, max_latency_ms, error_rate_percent
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    std::lock_guard<std::mutex> lock(m_mutex);
    MYSQL_STMT* stmt = mysql_stmt_init(m_conn);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned long rLen = static_cast<unsigned long>(run.run_id.size());
    unsigned long nLen = static_cast<unsigned long>(run.test_name.size());

    MYSQL_BIND bind[14];
    std::memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = const_cast<char*>(run.run_id.data());
    bind[0].length = &rLen;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = const_cast<char*>(run.test_name.data());
    bind[1].length = &nLen;

    int u = run.concurrent_users;
    int d = run.duration_seconds;
    int tr = run.total_requests;
    int sr = run.successful_requests;
    int fr = run.failed_requests;
    double rps = run.requests_per_second;
    double avg = run.avg_latency_ms;
    double p50 = run.p50_latency_ms;
    double p95 = run.p95_latency_ms;
    double p99 = run.p99_latency_ms;
    double maxL = run.max_latency_ms;
    double errR = run.error_rate_percent;

    bind[2].buffer_type = MYSQL_TYPE_LONG;   bind[2].buffer = &u;
    bind[3].buffer_type = MYSQL_TYPE_LONG;   bind[3].buffer = &d;
    bind[4].buffer_type = MYSQL_TYPE_LONG;   bind[4].buffer = &tr;
    bind[5].buffer_type = MYSQL_TYPE_LONG;   bind[5].buffer = &sr;
    bind[6].buffer_type = MYSQL_TYPE_LONG;   bind[6].buffer = &fr;
    bind[7].buffer_type = MYSQL_TYPE_DOUBLE; bind[7].buffer = &rps;
    bind[8].buffer_type = MYSQL_TYPE_DOUBLE; bind[8].buffer = &avg;
    bind[9].buffer_type = MYSQL_TYPE_DOUBLE; bind[9].buffer = &p50;
    bind[10].buffer_type = MYSQL_TYPE_DOUBLE; bind[10].buffer = &p95;
    bind[11].buffer_type = MYSQL_TYPE_DOUBLE; bind[11].buffer = &p99;
    bind[12].buffer_type = MYSQL_TYPE_DOUBLE; bind[12].buffer = &maxL;
    bind[13].buffer_type = MYSQL_TYPE_DOUBLE; bind[13].buffer = &errR;

    mysql_stmt_bind_param(stmt, bind);
    int rc = mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);
    return (rc == 0);
}

int64_t DatabaseManager::getPlayerCount() {
    if (!open()) return 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (mysql_query(m_conn, "SELECT COUNT(*) FROM players;") != 0) return 0;
    MYSQL_RES* res = mysql_store_result(m_conn);
    if (!res) return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    int64_t count = (row && row[0]) ? std::stoll(row[0]) : 0;
    mysql_free_result(res);
    return count;
}

int64_t DatabaseManager::getEventCount() {
    if (!open()) return 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (mysql_query(m_conn, "SELECT COUNT(*) FROM game_events;") != 0) return 0;
    MYSQL_RES* res = mysql_store_result(m_conn);
    if (!res) return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    int64_t count = (row && row[0]) ? std::stoll(row[0]) : 0;
    mysql_free_result(res);
    return count;
}
