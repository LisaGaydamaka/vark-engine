#include "logger.h"
#include <cstdarg>
#include <ctime>
#include <windows.h>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const char* logFilePath) {
    if (m_initialized) return;
    // Create logs directory if needed
    CreateDirectoryA("logs", nullptr);
    fopen_s(&m_file, logFilePath, "w");
    m_initialized = true;
}

void Logger::log(LogLevel level, const char* format, ...) {
    if (!m_initialized) return;

    const char* levelStr = (level == LogLevel::Info) ? "[INFO] " :
                           (level == LogLevel::Warning) ? "[WARN] " : "[ERROR] ";

    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Console output
    printf("%s%s\n", levelStr, buffer);

    // File output with timestamp
    if (m_file) {
        time_t now = time(nullptr);
        tm tmInfo;
        localtime_s(&tmInfo, &now);
        fprintf(m_file, "[%02d:%02d:%02d] %s%s\n",
                tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec, levelStr, buffer);
        fflush(m_file);
    }
}

void Logger::shutdown() {
    if (m_file) { fclose(m_file); m_file = nullptr; }
    m_initialized = false;
}