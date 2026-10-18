#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
   public:

    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);

    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    EventLoop *getNextLoop();

    std::vector<EventLoop *> getAllLoops();

    bool started() const { return started_; }
    const std::string &name() const { return name_; }

   private:

    EventLoop
        *baseLoop_;
    bool started_;
    int next_;

    std::string name_;
    int numThreads_;

    std::vector<std::unique_ptr<EventLoopThread> >
        threads_;
    std::vector<EventLoop *> loops_;
};
