#pragma once

#include <atomic>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "../base/CurrentThread.h"
#include "../base/noncopyable.h"
#include "TimerQueue.h"

class Channel;
class Epoll;

// Owns one event loop and dispatches I/O and queued callbacks on its thread.
class EventLoop : noncopyable {
   public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();

    void quit();

    void wakeup();

    void updateChannel(Channel* channel);

    void removeChannel(Channel* channel);

    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

    void assertInLoopThread() {
        if (!isInLoopThread()) {
            abort();
        }
    }

    // Run immediately on the loop thread, or enqueue when called from another thread.
    void runInLoop(Functor cb);

    // Thread-safe task submission; queued work runs on this loop's thread.
    void queueInLoop(Functor cb);

    // Schedule relative to the monotonic clock; calls are safe from any thread.
    TimerId runAfter(double delay, std::function<void()> cb);
    // The first callback runs after one full interval.
    TimerId runEvery(double interval, std::function<void()> cb);
    void cancel(TimerId timerId);

   private:

    void handleRead();

    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_;

    const pid_t threadId_;

    std::unique_ptr<Epoll> ep_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_;
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;

    std::unique_ptr<TimerQueue> timerQueue_;

};
