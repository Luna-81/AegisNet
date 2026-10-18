#pragma once

#include <functional>
#include <string>

#include "LogStream.h"
#include "noncopyable.h"
#ifndef LIKELY_MACROS
#define LIKELY_MACROS

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

enum LogLevel { INFO, ERROR, FATAL, DEBUG };

class Logger : noncopyable {
   public:
    Logger(const char* file, int line, LogLevel level);
    ~Logger();

    LogStream& stream() { return stream_; }

    static LogLevel& globalLogLevel();
    static void setLogLevel(LogLevel level);

    using OutputFunc = std::function<void(const char* msg, int len)>;
    using FlushFunc = void (*)();

    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);

   private:
    LogStream stream_;
    const char* file_;
    int line_;
    LogLevel level_;
};

#define LOG_INFO                          \
    if (Logger::globalLogLevel() <= INFO) \
    Logger(__FILE__, __LINE__, INFO).stream()

#define LOG_ERROR                          \
    if (Logger::globalLogLevel() <= ERROR) \
    Logger(__FILE__, __LINE__, ERROR).stream()

#define LOG_FATAL Logger(__FILE__, __LINE__, FATAL).stream()

#ifdef MUDEBUG
#define LOG_DEBUG                          \
    if (Logger::globalLogLevel() <= DEBUG) \
    Logger(__FILE__, __LINE__, DEBUG).stream()
#else
#define LOG_DEBUG \
    if (0) Logger(__FILE__, __LINE__, DEBUG).stream()
#endif