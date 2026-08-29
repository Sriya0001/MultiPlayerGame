#include "simulator/Simulator.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <nlohmann/json.hpp>

static Simulator* g_simulator = nullptr;

static void signalHandler(int) {
    if (g_simulator) {
        g_simulator->stop();
    }
}

static void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --host      <addr>   Server IP or hostname     (default: 127.0.0.1)\n"
              << "  --port      <port>   Server TCP port           (default: 7777)\n"
              << "  --players   <num>    Number of virtual players (default: 50)\n"
              << "  --duration  <sec>    Test duration in seconds  (default: 10)\n"
              << "  --interval  <ms>     Interval between actions  (default: 50)\n"
              << "  --base-id   <id>     Base player ID offset     (default: 1000)\n"
              << "  --seed      <seed>   Random seed               (default: 42)\n"
              << "  --json-out  <file>   Output summary JSON file  (optional)\n"
              << "  --help               Show this help\n";
}

int main(int argc, char* argv[]) {
    SimulatorConfig config;
    std::string jsonOutputFile;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--players" && i + 1 < argc) {
            config.playerCount = std::atoi(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            config.durationSeconds = std::atoi(argv[++i]);
        } else if (arg == "--interval" && i + 1 < argc) {
            config.actionIntervalMs = std::atoi(argv[++i]);
        } else if (arg == "--base-id" && i + 1 < argc) {
            config.basePlayerId = std::atoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            config.seed = std::atoi(argv[++i]);
        } else if (arg == "--json-out" && i + 1 < argc) {
            jsonOutputFile = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Simulator simulator(config);
    g_simulator = &simulator;

    auto summary = simulator.run();

    if (!jsonOutputFile.empty()) {
        nlohmann::json j = {
            {"total_requests",       summary.totalRequests},
            {"successful_requests",  summary.successfulRequests},
            {"failed_requests",      summary.failedRequests},
            {"duration_seconds",     summary.durationSeconds},
            {"requests_per_second",  summary.requestsPerSec},
            {"avg_latency_ms",       summary.avgLatencyMs},
            {"p50_latency_ms",       summary.p50LatencyMs},
            {"p95_latency_ms",       summary.p95LatencyMs},
            {"p99_latency_ms",       summary.p99LatencyMs},
            {"max_latency_ms",       summary.maxLatencyMs},
            {"error_rate_percent",   summary.errorRatePercent},
            {"player_count",         config.playerCount}
        };

        std::ofstream ofs(jsonOutputFile);
        if (ofs.is_open()) {
            ofs << j.dump(2) << std::endl;
            std::cout << "[Simulator] Saved performance JSON report to: " << jsonOutputFile << "\n";
        }
    }

    return (summary.totalRequests > 0) ? 0 : 1;
}
