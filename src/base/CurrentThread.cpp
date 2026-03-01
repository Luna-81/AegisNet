// CurrentThread.cpp
#include "CurrentThread.h"

#include <sys/syscall.h>
#include <unistd.h>

#include <cstdio>

namespace CurrentThread {
// 定义线程局部变量 (TLS)
__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread int t_tidStringLength = 6;
__thread const char* t_threadName = "unknown";

// 封装系统调用获取线程 ID
// 注意：gettid() 不是 POSIX 标准，但在 Linux 下非常有用
pid_t gettid() { return static_cast<pid_t>(::syscall(SYS_gettid)); }

void cacheTid() {
    if (t_cachedTid == 0) {
        t_cachedTid = gettid();

        // 把整数 ID 格式化成字符串，方便后续日志输出
        t_tidStringLength =
            snprintf(t_tidString, sizeof t_tidString, "%d", t_cachedTid);
    }
}
}  // namespace CurrentThread