#pragma once

#include "simulator/SimulatorConfig.h"
#include "simulator/MetricsCollector.h"
#include <atomic>

class Simulator {
public:
    explicit Simulator(const SimulatorConfig& config);

    PerformanceSummary run();

    void stop();

private:
    SimulatorConfig   m_config;
    MetricsCollector  m_collector;
    std::atomic<bool> m_stopFlag{false};
};
