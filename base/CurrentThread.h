// CurrentThread.h
#pragma once

#include <cstdint>

namespace CurrentThread {
// 1. 核心内部变量
// __thread 是 GCC 内置的线程局部存储 (TLS)
// 每个线程都有自己独立的 t_cachedTid，互不干扰
extern __thread int t_cachedTid;       // 线程局部存储变量。
extern __thread char t_tidString[32];  // 用于日志打印的字符串形式
extern __thread int t_tidStringLength;
extern __thread const char* t_threadName;

// 2. 核心接口
void cacheTid();  // 真正的系统调用，去获取 ID

// 获取当前线程 ID (热点路径，必须快，所以是 inline)
// tid() 函数：如何只在第一次调用时进行系统调用
// (syscall(SYS_gettid))，后面都直接读缓存。这是 EventLoop::isInLoopThread
// 能够高效运行的基础。
inline int tid() {
    // __builtin_expect 告诉编译器：t_cachedTid == 0 的情况极少发生
    // (分支预测优化)
    if (__builtin_expect(t_cachedTid == 0, 0)) {
        cacheTid();  // 只有第一次访问时才去系统获取
    }
    return t_cachedTid;
}

// 获取字符串形式的 ID ("1234")，打日志用
inline const char* tidString() { return t_tidString; }

inline int tidStringLength() { return t_tidStringLength; }

inline const char* name() { return t_threadName; }
}  // namespace CurrentThread