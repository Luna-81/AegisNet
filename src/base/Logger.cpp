#include "Logger.h"

#include <cstdio>  // for fwrite, stdout, fflush
#include <cstdlib>
#include <ctime>

// ==========================================

// ==========================================
void defaultOutput(const char* msg, int len) { fwrite(msg, 1, len, stdout); }

void defaultFlush() { fflush(stdout); }

Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

void Logger::setOutput(OutputFunc out) { g_output = out; }
void Logger::setFlush(FlushFunc flush) { g_flush = flush; }

// ==========================================

LogLevel& Logger::globalLogLevel() {
    static LogLevel level = INFO;
    return level;
}

void Logger::setLogLevel(LogLevel level) { globalLogLevel() = level; }

Logger::Logger(const char* file, int line, LogLevel level)
    : file_(file), line_(line), level_(level) {
    time_t now = time(nullptr);
    char buf[64] = {0};
    struct tm localTime {};
    if (::localtime_r(&now, &localTime) != nullptr) {
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTime);
    } else {
        std::snprintf(buf, sizeof(buf), "unknown-time");
    }

    stream_ << "[" << buf << "] ";

    switch (level) {
        case INFO:
            stream_ << "[INFO] ";
            break;
        case ERROR:
            stream_ << "[ERROR] ";
            break;
        case FATAL:
            stream_ << "[FATAL] ";
            break;
        case DEBUG:
            stream_ << "[DEBUG] ";
            break;
    }
}

Logger::~Logger() {
    stream_ << " - " << file_ << ":" << line_ << '\n';

    const LogStream::Buffer& buf(stream_.buffer());

    // AsyncLogging)
    g_output(buf.data(), buf.length());

    if (level_ == FATAL) {
        g_flush();
        abort();
    }
}
