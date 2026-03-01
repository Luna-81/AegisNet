//
// Created by luna on 2/22/26.
//

#pragma once

#include "LogStream.h"
#include "Thread.h"
// #include "noncopyable.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Singleton.h"  // day17 新增🔥 1. 引入单例模板
// day17 新增🔥 2. 继承 Singleton，把 AsyncLogging 传进去
class AsyncLogging : public Singleton<AsyncLogging> {
    // day17 新增🔥 3. 声明友元，让 Singleton 里的 getInstance()
    // 能够调用我们的私有构造函数
    friend class Singleton<AsyncLogging>;

   public:
    ~AsyncLogging() {
        if (running_) {
            stop();
        }
    }

    void append(
        const char* logline,
        int len);  // 作用：前台揽货窗口。主线程调用它，把几百字节的日志塞进
                   // currentBuffer_。如果满了，就放进 buffers_ 并按对讲机
                   // cond_.notify_one()。它必须极快，绝不碰磁盘。
    void start();  // start() 让后台司机开始干活
    void
    stop();  // 会按响对讲机把司机叫醒，等他把手里最后一点货全卸完（thread_.join()），然后下班。

   private:
    // 🔥 4. 没收出生证！把构造函数移到 private 里，并给一个默认的名字！
    // 这样 static AsyncLogging instance; 就能成功初始化了。
    AsyncLogging(const std::string& basename = "myserver_log",
                 int flushInterval = 3);

    void threadFunc();  // 作用：后台司机的绝活（核心引擎）
    // 1. 缓冲区指针 (Buffer Pointers)
    using Buffer = FixedBuffer<
        detail::
            kLargeBuffer>;  // 作用：定义了一个别名，代表一个 4MB
                            // 大小的内存块。这就是我们运货的**“标准集装箱”**。
    using BufferPtr = std::unique_ptr<Buffer>;
    using BufferVector = std::vector<BufferPtr>;

    BufferPtr
        currentBuffer_;  // 作用：前台当前正在使用的集装箱。业务员（主线程）调用的所有
                         // LOG_INFO，第一时间都是往这里塞。
    BufferPtr
        nextBuffer_;  // 作用：前台的“备胎”集装箱。当 currentBuffer_
                      // 瞬间被塞满时，业务员不用等，立刻把这个“备胎”拉过来变成新的
                      // currentBuffer_ 继续装货。
    BufferVector
        buffers_;  // 作用：满载集装箱的停泊区（待转运队列）。前台业务员把装满的集装箱扔到这里，等待后台司机拉走。
    // 2. 线程与同步控制 (Synchronization)
    std::atomic<bool>
        running_;  // 作用：总开关。控制后台线程是否继续死循环。原子操作保证多线程读写它的安全性。
    Thread thread_;  // 作用：后台司机本人。它是一个真正的操作系统线程，专门运行
                     // threadFunc() 函数里的死循环逻辑。
    std::mutex mutex_;  // 作用：保护 buffers_（停泊区）
    std::condition_variable
        cond_;  // 作用：前台业务员手里的**“对讲机”**。当前台把一个装满的箱子扔进
                // buffers_ 时，会调用 cond_.notify_one()
                // 大喊一声：“后台醒醒，有满箱子了，快来拉货！”

    // 3. 配置与杂项变量
    std::string basename_;  // 作用：日志文件名的前缀（比如
                            // "my_server"）。后台写磁盘时，会用它生成
                            // my_server.20260222-180000.log 这样的文件。
    const int flushInterval_;  // 作用：定时强制发车的倒计时（比如 3 秒）
};