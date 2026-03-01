#include "Logger.h"

#include <cstdio>  // for fwrite, stdout, fflush
#include <cstdlib>
#include <ctime>

// ==========================================
// 默认输出行为：写到屏幕终端 (在没有配置 AsyncLogging 时使用)
// ==========================================
void defaultOutput(const char* msg, int len) { fwrite(msg, 1, len, stdout); }

void defaultFlush() { fflush(stdout); }

// 全局输出回调指针，默认指向写屏幕
Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

// 🔥 核心改变 4：实现 setOutput 函数
void Logger::setOutput(OutputFunc out) { g_output = out; }
void Logger::setFlush(FlushFunc flush) { g_flush = flush; }

// ==========================================

LogLevel& Logger::globalLogLevel() {
    static LogLevel level = INFO;
    return level;
}

void Logger::setLogLevel(LogLevel level) { globalLogLevel() = level; }

// 构造函数：把时间和级别塞进 4KB 数组
Logger::Logger(const char* file, int line, LogLevel level)
    : file_(file), line_(line), level_(level) {
    time_t now = time(nullptr);
    char buf[64] = {0};
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // 全改成用 stream_ (LogStream) 写入！绝不触发系统级 IO
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

// 析构函数：完成单条日志，并呼叫全局回调把数据扔出去！
Logger::~Logger() {
    stream_ << " - " << file_ << ":" << line_ << '\n';

    // 取出写满了底层格式化数据的 4KB Buffer
    const LogStream::Buffer& buf(stream_.buffer());

    // 🔥 核心改变 5：将数据交给全局钩子 (在 main 里，这会被重定向到
    // AsyncLogging)
    g_output(buf.data(), buf.length());

    if (level_ == FATAL) {
        g_flush();
        abort();  // FATAL 级别产生 core dump 方便调试
    }
}