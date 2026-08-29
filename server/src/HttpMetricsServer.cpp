#include "server/HttpMetricsServer.h"
#include "server/Metrics.h"
#include "server/Logger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

HttpMetricsServer::HttpMetricsServer(uint16_t port, const std::string& host)
    : m_host(host), m_port(port)
{
}

HttpMetricsServer::~HttpMetricsServer() {
    stop();
}

bool HttpMetricsServer::start() {
    m_listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        Logger::error("HttpMetricsServer socket() failed: " + std::string(std::strerror(errno)));
        return false;
    }

    int opt = 1;
    ::setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    if (::inet_pton(AF_INET, m_host.c_str(), &addr.sin_addr) <= 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (::bind(m_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        Logger::error("HttpMetricsServer bind() failed on port " + std::to_string(m_port) + ": " + std::strerror(errno));
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    if (::listen(m_listenFd, 64) < 0) {
        Logger::error("HttpMetricsServer listen() failed: " + std::string(std::strerror(errno)));
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    m_running.store(true);
    m_thread = std::thread(&HttpMetricsServer::listenLoop, this);
    Logger::info("HttpMetricsServer started: http://" + m_host + ":" + std::to_string(m_port) + "/metrics");
    return true;
}

void HttpMetricsServer::stop() {
    if (!m_running.load()) return;
    m_running.store(false);

    if (m_listenFd >= 0) {
        ::shutdown(m_listenFd, SHUT_RDWR);
        ::close(m_listenFd);
        m_listenFd = -1;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void HttpMetricsServer::listenLoop() {
    while (m_running.load()) {
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);

        int clientFd = ::accept(m_listenFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            if (!m_running.load()) break;
            continue;
        }

        handleHttpClient(clientFd);
    }
}

void HttpMetricsServer::handleHttpClient(int clientFd) {
    char buf[2048] = {0};
    ssize_t r = ::recv(clientFd, buf, sizeof(buf) - 1, 0);
    if (r <= 0) {
        ::close(clientFd);
        return;
    }

    std::string req(buf, static_cast<size_t>(r));
    std::string response;

    if (req.find("GET /metrics") != std::string::npos) {
        std::string body = MetricsRegistry::instance().toPrometheusText();
        std::ostringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Connection: close\r\n\r\n"
           << body;
        response = ss.str();
    } else if (req.find("GET /health") != std::string::npos) {
        std::string body = "{\"status\":\"UP\"}\n";
        std::ostringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: application/json\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Connection: close\r\n\r\n"
           << body;
        response = ss.str();
    } else {
        std::string body = "Not Found. Use /metrics or /health\n";
        std::ostringstream ss;
        ss << "HTTP/1.1 404 Not Found\r\n"
           << "Content-Type: text/plain\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Connection: close\r\n\r\n"
           << body;
        response = ss.str();
    }

    ::send(clientFd, response.data(), response.size(), MSG_NOSIGNAL);
    ::close(clientFd);
}
