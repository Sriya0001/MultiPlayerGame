#include "server/Server.h"
#include "server/Logger.h"
#include "server/MessageFramer.h"

#include <nlohmann/json.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <stdexcept>
#include <thread>

using json = nlohmann::json;

// ── Construction / destruction ────────────────────────────────────────────────

Server::Server(const ServerConfig& config)
    : m_config(config)
    , m_listenFd(-1)
    , m_running(false)
    , m_cacheManager(std::make_shared<CacheManager>(config.enableRedis, config.redisHost, config.redisPort))
    , m_dbManager(std::make_shared<DatabaseManager>(config.dbHost, config.dbPort, config.dbUser, config.dbPass, config.dbName))
    , m_requestHandler(m_cacheManager, m_dbManager)
{
    if (m_dbManager) {
        m_dbManager->initSchema("database/schema_mysql.sql");
    }
}

Server::~Server() {
    stop();
    if (m_pool)  m_pool->stop();
    if (m_listenFd >= 0) {
        ::close(m_listenFd);
        m_listenFd = -1;
    }
}

// ── Public interface ──────────────────────────────────────────────────────────

void Server::stop() {
    m_running.store(false);
    if (m_httpMetricsServer) {
        m_httpMetricsServer->stop();
    }
    // Unblock accept() by closing the listen socket
    if (m_listenFd >= 0) {
        ::shutdown(m_listenFd, SHUT_RDWR);
    }
}

void Server::run() {
    if (!setupListenSocket()) {
        Logger::error("Failed to set up listen socket — aborting.");
        return;
    }

    // Start Prometheus HTTP Metrics Server on dedicated port
    m_httpMetricsServer = std::make_unique<HttpMetricsServer>(m_config.metricsPort);
    m_httpMetricsServer->start();

    // Determine thread pool size
    int numThreads = m_config.threadPoolSize;
    if (numThreads <= 0) {
        numThreads = static_cast<int>(std::thread::hardware_concurrency());
        if (numThreads <= 0) numThreads = 4;
        // Cap at 32 for now; later milestones will tune based on benchmarks
        if (numThreads > 32) numThreads = 32;
    }

    // Create thread pool — handler lambda captures 'this'
    m_pool = std::make_unique<ThreadPool>(
        numThreads,
        [this](int fd, const std::string& addr) {
            this->handleClient(fd, addr);
        }
    );

    m_running.store(true);
    Logger::info("Game server listening on "
                 + m_config.host + ":" + std::to_string(m_config.port)
                 + "  threads=" + std::to_string(numThreads));

    // ── Accept loop (main thread) ─────────────────────────────────────────────
    while (m_running.load()) {
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);

        int clientFd = ::accept(m_listenFd,
                                reinterpret_cast<sockaddr*>(&clientAddr),
                                &clientLen);
        if (clientFd < 0) {
            if (!m_running.load()) break;
            Logger::warn("accept() failed: " + std::string(std::strerror(errno)));
            continue;
        }

        char ipBuf[INET_ADDRSTRLEN] = {};
        ::inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));
        std::string clientAddrStr =
            std::string(ipBuf) + ":" + std::to_string(ntohs(clientAddr.sin_port));

        // Hand off to thread pool — returns immediately
        m_pool->submit(clientFd, clientAddrStr);
    }

    // Drain the pool before returning
    if (m_pool) m_pool->stop();
    Logger::info("Server run loop exited.");
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool Server::setupListenSocket() {
    m_listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        Logger::error("socket() failed: " + std::string(std::strerror(errno)));
        return false;
    }

    int opt = 1;
    ::setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(m_config.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(m_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        Logger::error("bind() failed: " + std::string(std::strerror(errno)));
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    if (::listen(m_listenFd, m_config.backlog) < 0) {
        Logger::error("listen() failed: " + std::string(std::strerror(errno)));
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    return true;
}

void Server::handleClient(int clientFd, const std::string& clientAddr) {
    uint64_t connId = m_connManager.registerConnection(clientFd, clientAddr);

    Logger::info("Client connected: id=" + std::to_string(connId)
                 + " addr=" + clientAddr
                 + " active=" + std::to_string(m_connManager.activeCount()));

    uint8_t  lenBuf[MessageFramer::HEADER_SIZE];
    uint32_t msgCount = 0;

    while (true) {
        if (!MessageFramer::readExact(clientFd, lenBuf, sizeof(lenBuf))) break;

        uint32_t payloadLen =
              static_cast<uint32_t>(lenBuf[0])
            | (static_cast<uint32_t>(lenBuf[1]) << 8)
            | (static_cast<uint32_t>(lenBuf[2]) << 16)
            | (static_cast<uint32_t>(lenBuf[3]) << 24);

        if (payloadLen > MessageFramer::MAX_MESSAGE_SIZE) {
            Logger::warn("conn=" + std::to_string(connId)
                         + " message too large — closing.");
            break;
        }

        std::string payload(payloadLen, '\0');
        if (!MessageFramer::readExact(clientFd, payload.data(), payloadLen)) break;

        m_connManager.recordMessageReceived(connId);
        ++msgCount;

        Logger::debug("conn=" + std::to_string(connId)
                      + " rx[" + std::to_string(msgCount) + "]: " + payload);

        std::string responseJson = dispatch(payload, connId);
        auto frame = MessageFramer::encode(responseJson);

        if (!MessageFramer::writeAll(clientFd, frame)) {
            Logger::warn("conn=" + std::to_string(connId) + " send failed.");
            break;
        }

        m_connManager.recordMessageSent(connId);

        Logger::debug("conn=" + std::to_string(connId)
                      + " tx[" + std::to_string(msgCount) + "]: " + responseJson);
    }

    ::close(clientFd);
    m_connManager.unregisterConnection(connId);

    Logger::info("Client disconnected: id=" + std::to_string(connId)
                 + " msgs=" + std::to_string(msgCount)
                 + " active=" + std::to_string(m_connManager.activeCount()));
}

std::string Server::dispatch(const std::string& rawJson, uint64_t connId) {
    return m_requestHandler.handle(rawJson, connId);
}
