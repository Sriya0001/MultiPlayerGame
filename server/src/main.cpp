#include "server/Server.h"
#include "server/Logger.h"

#include <cstdlib>
#include <csignal>
#include <string>
#include <iostream>

// ── Global server pointer for signal handler ──────────────────────────────────
// Using a raw pointer here is intentional: signal handlers cannot safely
// interact with C++ objects in most cases, so we only set an atomic flag
// through the Server::stop() call, which is signal-safe.
static Server* g_server = nullptr;

static void signalHandler(int signum) {
    if (g_server) {
        g_server->stop();
    }
    // Do not call Logger here; async-signal-safety rules prohibit it.
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --port          <port>     TCP port to listen on    (default: 7777)\n"
              << "  --host          <addr>     Bind address              (default: 0.0.0.0)\n"
              << "  --threads       <n>        Thread pool size          (default: auto)\n"
              << "  --enable-redis             Enable Redis cache layer  (default: disabled)\n"
              << "  --redis-host    <addr>     Redis server host         (default: 127.0.0.1)\n"
              << "  --redis-port    <port>     Redis server port         (default: 6379)\n"
              << "  --db-host       <addr>     MySQL host                (default: 127.0.0.1)\n"
              << "  --db-port       <port>     MySQL port                (default: 3306)\n"
              << "  --db-user       <user>     MySQL username            (default: game_user)\n"
              << "  --db-pass       <pass>     MySQL password            (default: game_pass)\n"
              << "  --db-name       <name>     MySQL database            (default: game_server)\n"
              << "  --metrics-port  <port>     Prometheus HTTP port      (default: 9100)\n"
              << "  --debug                    Enable DEBUG-level logs\n"
              << "  --help                     Show this help\n";
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    ServerConfig config;

    // Simple argument parsing — no external dependency needed at this stage.
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--debug") {
            Logger::setLevel(LogLevel::DEBUG);
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            config.threadPoolSize = std::atoi(argv[++i]);
        } else if (arg == "--enable-redis") {
            config.enableRedis = true;
        } else if (arg == "--redis-host" && i + 1 < argc) {
            config.redisHost = argv[++i];
        } else if (arg == "--redis-port" && i + 1 < argc) {
            config.redisPort = std::atoi(argv[++i]);
        } else if (arg == "--db-host" && i + 1 < argc) {
            config.dbHost = argv[++i];
        } else if (arg == "--db-port" && i + 1 < argc) {
            config.dbPort = std::atoi(argv[++i]);
        } else if (arg == "--db-user" && i + 1 < argc) {
            config.dbUser = argv[++i];
        } else if (arg == "--db-pass" && i + 1 < argc) {
            config.dbPass = argv[++i];
        } else if (arg == "--db-name" && i + 1 < argc) {
            config.dbName = argv[++i];
        } else if (arg == "--metrics-port" && i + 1 < argc) {
            config.metricsPort = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    Logger::info("Starting Game Server v0.1.0 (Milestone 1)");

    Server server(config);
    g_server = &server;

    // Graceful shutdown on SIGINT (Ctrl-C) and SIGTERM (Docker/systemd stop).
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    server.run();

    Logger::info("Server shut down cleanly.");
    return 0;
}
