#include "server/RedisClient.h"
#include "server/Logger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

// ── RedisConnection ──────────────────────────────────────────────────────────

RedisConnection::RedisConnection(const std::string& host, int port, int timeoutMs)
    : m_host(host), m_port(port), m_timeoutMs(timeoutMs), m_socketFd(-1)
{
}

RedisConnection::~RedisConnection() {
    disconnect();
}

bool RedisConnection::connect() {
    if (m_socketFd >= 0) return true;

    m_socketFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFd < 0) return false;

    // Set timeout
    timeval tv;
    tv.tv_sec = m_timeoutMs / 1000;
    tv.tv_usec = (m_timeoutMs % 1000) * 1000;
    ::setsockopt(m_socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(m_socketFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(m_port));
    if (::inet_pton(AF_INET, m_host.c_str(), &addr.sin_addr) <= 0) {
        disconnect();
        return false;
    }

    if (::connect(m_socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        disconnect();
        return false;
    }

    return true;
}

void RedisConnection::disconnect() {
    if (m_socketFd >= 0) {
        ::close(m_socketFd);
        m_socketFd = -1;
    }
}

bool RedisConnection::sendCommand(const std::vector<std::string>& args) {
    if (!connect()) return false;

    // Format RESP Array: *<count>\r\n$<len>\r\n<arg>\r\n...
    std::ostringstream ss;
    ss << "*" << args.size() << "\r\n";
    for (const auto& arg : args) {
        ss << "$" << arg.size() << "\r\n" << arg << "\r\n";
    }

    std::string wire = ss.str();
    ssize_t sent = ::send(m_socketFd, wire.data(), wire.size(), MSG_NOSIGNAL);
    if (sent != static_cast<ssize_t>(wire.size())) {
        disconnect();
        return false;
    }
    return true;
}

bool RedisConnection::readLine(std::string& line) {
    line.clear();
    char c = 0;
    while (true) {
        ssize_t r = ::recv(m_socketFd, &c, 1, 0);
        if (r <= 0) {
            disconnect();
            return false;
        }
        if (c == '\r') {
            char next = 0;
            if (::recv(m_socketFd, &next, 1, 0) <= 0 || next != '\n') {
                disconnect();
                return false;
            }
            break;
        }
        line.push_back(c);
    }
    return true;
}

bool RedisConnection::readBulkString(std::string& out) {
    std::string lenStr;
    if (!readLine(lenStr)) return false;

    int len = std::stoi(lenStr);
    if (len < 0) {
        // Null bulk string (key not found)
        return false;
    }

    out.resize(len);
    size_t total = 0;
    while (total < static_cast<size_t>(len)) {
        ssize_t r = ::recv(m_socketFd, &out[total], len - total, 0);
        if (r <= 0) {
            disconnect();
            return false;
        }
        total += r;
    }

    // Read trailing \r\n
    char crlf[2];
    if (::recv(m_socketFd, crlf, 2, MSG_WAITALL) != 2) {
        disconnect();
        return false;
    }
    return true;
}

bool RedisConnection::ping() {
    if (!sendCommand({"PING"})) return false;
    std::string line;
    if (!readLine(line)) return false;
    return (line == "+PONG" || line == "PONG");
}

bool RedisConnection::set(const std::string& key, const std::string& value, int expireSeconds) {
    std::vector<std::string> args = {"SET", key, value};
    if (expireSeconds > 0) {
        args.push_back("EX");
        args.push_back(std::to_string(expireSeconds));
    }
    if (!sendCommand(args)) return false;
    std::string line;
    if (!readLine(line)) return false;
    return (line == "+OK" || line == "OK");
}

bool RedisConnection::get(const std::string& key, std::string& outValue) {
    if (!sendCommand({"GET", key})) return false;
    char type = 0;
    if (::recv(m_socketFd, &type, 1, 0) <= 0) {
        disconnect();
        return false;
    }
    if (type == '$') {
        return readBulkString(outValue);
    }
    std::string rest;
    readLine(rest);
    return false;
}

bool RedisConnection::del(const std::string& key) {
    if (!sendCommand({"DEL", key})) return false;
    std::string line;
    return readLine(line);
}

bool RedisConnection::hset(const std::string& key, const std::string& field, const std::string& value) {
    if (!sendCommand({"HSET", key, field, value})) return false;
    std::string line;
    return readLine(line);
}

bool RedisConnection::hget(const std::string& key, const std::string& field, std::string& outValue) {
    if (!sendCommand({"HGET", key, field})) return false;
    char type = 0;
    if (::recv(m_socketFd, &type, 1, 0) <= 0) {
        disconnect();
        return false;
    }
    if (type == '$') {
        return readBulkString(outValue);
    }
    std::string rest;
    readLine(rest);
    return false;
}

bool RedisConnection::hgetall(const std::string& key, std::unordered_map<std::string, std::string>& outMap) {
    outMap.clear();
    if (!sendCommand({"HGETALL", key})) return false;
    char type = 0;
    if (::recv(m_socketFd, &type, 1, 0) <= 0) {
        disconnect();
        return false;
    }
    if (type != '*') {
        std::string rest;
        readLine(rest);
        return false;
    }

    std::string countStr;
    if (!readLine(countStr)) return false;
    int count = std::stoi(countStr);
    if (count <= 0) return true;

    for (int i = 0; i < count; i += 2) {
        char kType = 0, vType = 0;
        if (::recv(m_socketFd, &kType, 1, 0) <= 0) return false;
        std::string k, v;
        if (!readBulkString(k)) return false;
        if (::recv(m_socketFd, &vType, 1, 0) <= 0) return false;
        if (!readBulkString(v)) return false;
        outMap[k] = v;
    }
    return true;
}

// ── RedisPool ────────────────────────────────────────────────────────────────

RedisPool::RedisPool(const std::string& host, int port, size_t poolSize)
    : m_host(host), m_port(port), m_poolSize(poolSize)
{
}

RedisPool::~RedisPool() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_available.clear();
}

std::unique_ptr<RedisPool::PooledConnection> RedisPool::acquire() {
    std::unique_ptr<RedisConnection> conn;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_available.empty()) {
            conn = std::move(m_available.back());
            m_available.pop_back();
        }
    }

    if (!conn) {
        conn = std::make_unique<RedisConnection>(m_host, m_port);
    }

    if (!conn->isConnected()) {
        conn->connect();
    }

    return std::make_unique<PooledConnection>(*this, std::move(conn));
}

void RedisPool::release(std::unique_ptr<RedisConnection> conn) {
    if (!conn) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_available.size() < m_poolSize) {
        m_available.push_back(std::move(conn));
    }
}

bool RedisPool::isAvailable() {
    auto conn = acquire();
    return conn && conn->get()->ping();
}

RedisPool::PooledConnection::PooledConnection(RedisPool& pool, std::unique_ptr<RedisConnection> conn)
    : m_pool(pool), m_conn(std::move(conn))
{
}

RedisPool::PooledConnection::~PooledConnection() {
    m_pool.release(std::move(m_conn));
}
