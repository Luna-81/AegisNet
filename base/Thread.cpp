#include "Thread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false), joined_(false), func_(std::move(func)), name_(name) {
    setDefaultName();
}

Thread::~Thread() {
    if (started_ && !joined_) {
        thread_->detach();  // 🔥 修复崩溃：如果没 join，析构时自动 detach
    }
}

void Thread::start() {
    started_ = true;
    // 使用 lambda 启动线程
    thread_ = std::make_shared<std::thread>([this]() {
        // 这里可以加上获取 tid 的逻辑
        func_();
    });
}

void Thread::join() {
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName() {
    int num = ++numCreated_;
    if (name_.empty()) {
        char buf[32];
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}