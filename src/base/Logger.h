#pragma once

#include <functional>
#include <string>

#include "LogStream.h"  // 🔥 引入极速格式化引擎
#include "noncopyable.h"
#ifndef LIKELY_MACROS
#define LIKELY_MACROS
// 告诉编译器，这行代码大概率是 true (1) 还是 false (0)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

enum LogLevel { INFO, ERROR, FATAL, DEBUG };

class Logger : noncopyable {
   public:
    Logger(const char* file, int line, LogLevel level);
    ~Logger();

    // 🔥 核心改变 1：返回我们自己的 LogStream，而不是 std::ostream
    LogStream& stream() { return stream_; }

    static LogLevel& globalLogLevel();
    static void setLogLevel(LogLevel level);

    // 🔥 核心改变 2：声明全局输出钩子 (这就是你 main 函数里报红的原因)
    using OutputFunc = std::function<void(const char* msg, int len)>;
    using FlushFunc = void (*)();
    // TODO 本来发往终端的显示的，被setOutput发往了 AsyncLogging 写太磁盘
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);

   private:
    LogStream stream_;  // 🔥 核心改变 3：Logger 现在自带一个 4KB 的极速 Buffer
    const char* file_;
    int line_;
    LogLevel level_;
};

// 宏定义保持不变
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