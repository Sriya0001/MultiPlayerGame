#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
// RedisConnection
//
// Lightweight, direct RESP (REdis Serialization Protocol) socket client.
// Supports core caching commands with microsecond latency:
//   - PING
//   - GET / SET / DEL / EXPIRE
//   - HGET / HSET / HGETALL
// ─────────────────────────────────────────────────────────────────────────────

class RedisConnection {
public:
    RedisConnection(const std::string& host = "127.0.0.1", int port = 6379, int timeoutMs = 1000);
    ~RedisConnection();

    bool connect();
    void disconnect();
    bool isConnected() const { return m_socketFd >= 0; }

    bool ping();
    bool set(const std::string& key, const std::string& value, int expireSeconds = 0);
    bool get(const std::string& key, std::string& outValue);
    bool del(const std::string& key);

    bool hset(const std::string& key, const std::string& field, const std::string& value);
    bool hget(const std::string& key, const std::string& field, std::string& outValue);
    bool hgetall(const std::string& key, std::unordered_map<std::string, std::string>& outMap);

private:
    bool sendCommand(const std::vector<std::string>& args);
    bool readLine(std::string& line);
    bool readBulkString(std::string& out);
    bool readResponse(std::string& out);

    std::string m_host;
    int         m_port;
    int         m_timeoutMs;
    int         m_socketFd{-1};
};

// ─────────────────────────────────────────────────────────────────────────────
// RedisPool
//
// Thread-safe connection pool for concurrent worker threads.
// ─────────────────────────────────────────────────────────────────────────────

class RedisPool {
public:
    RedisPool(const std::string& host = "127.0.0.1", int port = 6379, size_t poolSize = 32);
    ~RedisPool();

    // RAII wrapper that acquires a connection from the pool and returns it on destruction
    class PooledConnection {
    public:
        PooledConnection(RedisPool& pool, std::unique_ptr<RedisConnection> conn);
        ~PooledConnection();
        RedisConnection* get() { return m_conn.get(); }
        RedisConnection* operator->() { return m_conn.get(); }
    private:
        RedisPool& m_pool;
        std::unique_ptr<RedisConnection> m_conn;
    };

    std::unique_ptr<PooledConnection> acquire();
    void release(std::unique_ptr<RedisConnection> conn);

    bool isAvailable();

private:
    std::string m_host;
    int         m_port;
    size_t      m_poolSize;
    std::mutex  m_mutex;
    std::vector<std::unique_ptr<RedisConnection>> m_available;
};
