#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>

#include "Channel.h"

class EventLoop;

using TimerId = uint64_t;

// Owns timers scheduled on one EventLoop and dispatches their callbacks there.
class TimerQueue {
   public:
    using TimerCallback = std::function<void()>;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // Returns zero for invalid arguments; nonzero IDs can be canceled later.
    TimerId addTimer(TimerCallback cb, double delay, double interval);
    void cancel(TimerId timerId);

   private:
    struct Timer;
    using Entry = std::pair<int64_t, std::shared_ptr<Timer>>;

    void addTimerInLoop(const std::shared_ptr<Timer>& timer, double delay);
    void cancelInLoop(TimerId timerId);
    void handleRead();
    void resetTimerfd(int64_t expiration);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    std::atomic<TimerId> nextTimerId_{1};
    std::set<Entry> timers_;
    std::map<TimerId, std::shared_ptr<Timer>> activeTimers_;
};
