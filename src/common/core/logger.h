#pragma once
#include <cstdio>
#include <string>

enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    static Logger& instance();
    void init(const char* logFilePath = "logs/vark.log");
    void log(LogLevel level, const char* format, ...);
    void shutdown();

private:
    Logger() = default;
    FILE* m_file = nullptr;
    bool m_initialized = false;
};

// Convenience macros
#define LOG_INFO(...)   Logger::instance().log(LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...)   Logger::instance().log(LogLevel::Warning, __VA_ARGS__)
#define LOG_ERROR(...)  Logger::instance().log(LogLevel::Error, __VA_ARGS__)