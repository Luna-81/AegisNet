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

    void
    threadFunc();

    EventLoop* loop_;
    bool exiting_;

    Thread thread_;

    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};