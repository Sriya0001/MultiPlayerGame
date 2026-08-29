#pragma once

#include "server/ConnectionManager.h"
#include "server/RequestHandler.h"
#include "server/ThreadPool.h"
#include "server/CacheManager.h"
#include "server/DatabaseManager.h"
#include "server/HttpMetricsServer.h"

#include <cstdint>
#include <atomic>
#include <string>
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// ServerConfig
// ─────────────────────────────────────────────────────────────────────────────
struct ServerConfig {
    std::string host           = "0.0.0.0";
    uint16_t    port           = 7777;
    uint16_t    metricsPort    = 9100;
    int         backlog        = 128;
    int         maxClients     = 4096;
    int         threadPoolSize = 0; // 0 = auto (hardware_concurrency)
    bool        enableRedis    = false;
    std::string redisHost      = "127.0.0.1";
    int         redisPort      = 6379;
    std::string dbHost         = "127.0.0.1";
    int         dbPort         = 3306;
    std::string dbUser         = "game_user";
    std::string dbPass         = "game_pass";
    std::string dbName         = "game_server";
};

// ─────────────────────────────────────────────────────────────────────────────
// Server
// ─────────────────────────────────────────────────────────────────────────────
class Server {
public:
    explicit Server(const ServerConfig& config);
    ~Server();

    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    void run();
    void stop();

    bool isRunning() const { return m_running.load(); }

    const ConnectionManager& connectionManager() const { return m_connManager; }
    const RequestHandler&    requestHandler()    const { return m_requestHandler; }
    std::shared_ptr<DatabaseManager> databaseManager() const { return m_dbManager; }

private:
    bool setupListenSocket();
    void handleClient(int clientFd, const std::string& clientAddr);
    std::string dispatch(const std::string& rawJson, uint64_t connId);

    ServerConfig      m_config;
    int               m_listenFd = -1;
    std::atomic<bool> m_running{false};

    ConnectionManager m_connManager;
    std::shared_ptr<CacheManager>    m_cacheManager;
    std::shared_ptr<DatabaseManager> m_dbManager;
    std::unique_ptr<HttpMetricsServer> m_httpMetricsServer;
    RequestHandler    m_requestHandler;

    // Thread pool is constructed in run() once we know the thread count.
    // Using unique_ptr so we can construct it after the Server object itself.
    std::unique_ptr<ThreadPool> m_pool;
};
