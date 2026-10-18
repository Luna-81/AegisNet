#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

// Small wrapper that stores a task and manages its std::thread lifecycle.
class Thread {
   public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string &name = std::string());

    ~Thread();

    void start();

    void join();

    bool started() const { return started_; }
    const std::string &name() const { return name_; }

   private:

    static std::atomic_int
        numCreated_;
    std::string name_;
    void
    setDefaultName();

    bool started_;
    bool joined_;

    std::shared_ptr<std::thread>
        thread_;
    ThreadFunc
        func_;

};
