#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// HttpMetricsServer
//
// Lightweight embedded HTTP server that exposes the /metrics endpoint
// for Prometheus scraper scraping.
// ─────────────────────────────────────────────────────────────────────────────

class HttpMetricsServer {
public:
    explicit HttpMetricsServer(uint16_t port = 9100, const std::string& host = "0.0.0.0");
    ~HttpMetricsServer();

    bool start();
    void stop();

    bool isRunning() const { return m_running.load(); }
    uint16_t port() const { return m_port; }

private:
    void listenLoop();
    void handleHttpClient(int clientFd);

    std::string       m_host;
    uint16_t          m_port;
    int               m_listenFd{-1};
    std::atomic<bool> m_running{false};
    std::thread       m_thread;
};
