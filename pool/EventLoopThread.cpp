#include "EventLoopThread.h"

#include "../net/EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string& name)
    : loop_(nullptr),
      exiting_(false),
      // ✅ 仅仅是初始化 Thread 对象，线程此时并未启动
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb) {}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_ != nullptr) {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    // 1. 启动线程：现在只会启动一次，不会有重复赋值的问题
    thread_.start();

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr) {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;

    if (callback_) {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();  // 阻塞在这里，直到 loop->quit() 被调用

    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
