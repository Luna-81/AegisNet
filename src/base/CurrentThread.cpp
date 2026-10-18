// CurrentThread.cpp
#include "CurrentThread.h"

#include <sys/syscall.h>
#include <unistd.h>

#include <cstdio>

namespace CurrentThread {

__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread int t_tidStringLength = 6;
__thread const char* t_threadName = "unknown";

pid_t gettid() { return static_cast<pid_t>(::syscall(SYS_gettid)); }

void cacheTid() {
    if (t_cachedTid == 0) {
        t_cachedTid = gettid();

        t_tidStringLength =
            snprintf(t_tidString, sizeof t_tidString, "%d", t_cachedTid);
    }
}
}  // namespace CurrentThread