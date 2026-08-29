#pragma once

#include "simulator/SimulatorConfig.h"
#include "simulator/MetricsCollector.h"
#include <random>
#include <atomic>
#include <string>

class PlayerBot {
public:
    PlayerBot(int playerId, const SimulatorConfig& config, MetricsCollector& collector);
    ~PlayerBot();

    void run(const std::atomic<bool>& stopFlag);

private:
    bool connectSocket();
    void disconnectSocket();
    bool sendAndReceive(const std::string& action, const std::string& jsonPayload);

    int              m_playerId;
    SimulatorConfig  m_config;
    MetricsCollector& m_collector;
    int              m_socketFd{-1};
    std::mt19937     m_rng;
    std::vector<RequestMetric> m_localMetrics;
};
