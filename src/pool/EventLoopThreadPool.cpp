//
// Created by luna on 2/17/26.
//

#include "EventLoopThreadPool.h"

#include "EventLoopThread.h"

// 修复点 1：构造函数参数必须和 .h 一模一样
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop,
                                         const std::string& nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {
    // 不需要手动 delete，因为使用了 unique_ptr 和 vector 自动管理
}

// TODO start 很重要
void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
    started_ = true;

    // 创建子线程
    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);

        // 创建 EventLoopThread 对象
        EventLoopThread* t = new EventLoopThread(cb, buf);

        // 存入 unique_ptr 容器
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));

        // 启动线程，并获取 loop 指针
        loops_.push_back(t->startLoop());
    }

    // 如果 numThreads 是 0，且只有 baseLoop，也要执行回调
    if (numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}

// 1. 更加高效的轮询分发
EventLoop* EventLoopThreadPool::getNextLoop() {
    EventLoop* loop = baseLoop_;

    // 只有在有多线程的情况下才进行轮询
    if (!loops_.empty()) {
        loop = loops_[next_];
        ++next_;
        // 用 if 替代 % 取模，性能更高
        if (static_cast<size_t>(next_) >= loops_.size()) {
            next_ = 0;
        }
    }
    return loop;
}

// 2. 获取所有 Loop (保留它，为了未来扩展)
std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    if (loops_.empty()) {
        // 如果没有子线程，就只有主 Loop
        return std::vector<EventLoop*>(1, baseLoop_);
    } else {
        // 如果有子线程，返回所有子 Loop
        return loops_;
    }
}