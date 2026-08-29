#include "simulator/Simulator.h"
#include "simulator/PlayerBot.h"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>

Simulator::Simulator(const SimulatorConfig& config)
    : m_config(config)
{
}

void Simulator::stop() {
    m_stopFlag.store(true);
}

PerformanceSummary Simulator::run() {
    std::cout << "[Simulator] Launching " << m_config.playerCount 
              << " virtual players targeting " << m_config.host << ":" << m_config.port 
              << " for " << m_config.durationSeconds << "s...\n";

    std::vector<std::thread> threads;
    threads.reserve(m_config.playerCount);

    auto tStart = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < m_config.playerCount; ++i) {
        int playerId = m_config.basePlayerId + i;
        threads.emplace_back([this, playerId]() {
            PlayerBot bot(playerId, m_config, m_collector);
            bot.run(m_stopFlag);
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    double durationSec = std::chrono::duration<double>(tEnd - tStart).count();

    auto summary = m_collector.computeSummary(durationSec);
    m_collector.printSummary(summary);
    return summary;
}
