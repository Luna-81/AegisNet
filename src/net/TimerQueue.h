#pragma once

#include <functional>
#include <set>
#include <vector>

#include "../base/Timestamp.h"
#include "Channel.h"

class EventLoop;

// Stores a callback and its next expiration time.
class Timer {
   public:
    using TimerCallback = std::function<void()>;
    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(std::move(cb)),
          expiration_(when),
          interval_(interval),
          repeat_(interval > 0.0),
          canceled_(false) {}
    void run() const {
        if (!canceled_) callback_();
    }
    Timestamp expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }

    void restart(Timestamp now) {
        if (repeat_) {

            expiration_ = addTime(now, interval_);
        } else {

            expiration_ = Timestamp();
        }
    }

    void cancel() { canceled_ = true; }
    bool isCanceled() const { return canceled_; }

   private:
    const TimerCallback callback_;
    Timestamp expiration_;
    const double interval_;
    const bool repeat_;

    bool canceled_;
};

// Runs expired timer callbacks from the owning EventLoop.
class TimerQueue {
   public:
    using TimerCallback = std::function<void()>;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    void addTimer(TimerCallback cb, Timestamp when, double interval);

   private:

    void handleRead();

    std::vector<Timer*> getExpired(Timestamp now);

    void resetTimerfd(int timerfd, Timestamp expiration);

    bool insert(Timer* timer);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;

    using Entry = std::pair<Timestamp, Timer*>;
    std::set<Entry> timers_;
};
