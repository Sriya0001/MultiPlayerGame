#pragma once

#include <string>
#include <atomic>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Logger
//
// Thread-safe, structured logger used throughout the server.
// Each line is emitted as:
//
//   [LEVEL] YYYY-MM-DD HH:MM:SS | <message>
//
// Levels: DEBUG < INFO < WARN < ERROR
// ─────────────────────────────────────────────────────────────────────────────

enum class LogLevel : int {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3
};

class Logger {
public:
    // Set the minimum level that gets emitted. Messages below this are dropped.
    static void setLevel(LogLevel level);

    static void debug(const std::string& msg);
    static void info (const std::string& msg);
    static void warn (const std::string& msg);
    static void error(const std::string& msg);

private:
    static void log(LogLevel level, const std::string& msg);
    static std::string levelToString(LogLevel level);
    static std::string currentTimestamp();

    static std::atomic<int> s_level; // stored as int for atomic ops
};
