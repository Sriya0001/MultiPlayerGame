#include "server/Logger.h"

#include <iostream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

// ── Static members ────────────────────────────────────────────────────────────
std::atomic<int> Logger::s_level{static_cast<int>(LogLevel::INFO)};

// ── Public API ────────────────────────────────────────────────────────────────

void Logger::setLevel(LogLevel level) {
    s_level.store(static_cast<int>(level));
}

void Logger::debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
void Logger::info (const std::string& msg) { log(LogLevel::INFO,  msg); }
void Logger::warn (const std::string& msg) { log(LogLevel::WARN,  msg); }
void Logger::error(const std::string& msg) { log(LogLevel::ERROR, msg); }

// ── Private helpers ───────────────────────────────────────────────────────────

void Logger::log(LogLevel level, const std::string& msg) {
    if (static_cast<int>(level) < s_level.load()) return;

    // Mutex ensures lines are not interleaved when threads are added later.
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> lock(s_mutex);

    std::cout << "[" << levelToString(level) << "] "
              << currentTimestamp()
              << " | " << msg << "\n";
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default:              return "?????";
    }
}

std::string Logger::currentTimestamp() {
    using namespace std::chrono;
    auto now     = system_clock::now();
    auto t       = system_clock::to_time_t(now);
    auto ms      = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << "." << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}
