#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#include "../base/Thread.h"

class EventLoop;

class EventLoopThread {
   public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                    const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();

   private:
    // 1. 核心逻辑 (The Script)
    void
    threadFunc();  // 分公司的运营手册/执行脚本 作用：这是子线程真正运行的函数。

    // 2. 核心资产 (The Product)
    EventLoop* loop_;  // 分公司的钥匙/地址  作用：这是主线程最想要的东西。
    bool exiting_;  // 倒闭状态. 作用：标志位。

    // 3.动力引擎 (The Engine)
    Thread thread_;  // 分公司的员工团队 / 物理载体。    作用：封装了底层的 OS
                     // 线程。

    // 4. 同步工具 (The Hotline)
    std::mutex mutex_;  // 互斥锁:为了保护共享数据不被乱改
    std::condition_variable cond_;  // 条件变量: 用于线程之间的等待和通知
    ThreadInitCallback callback_;  // 回调函数: 用户自定义的初始化代码
};