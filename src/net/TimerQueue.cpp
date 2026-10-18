#include "TimerQueue.h"

#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

#include "EventLoop.h"

namespace {

constexpr int64_t kMicrosecondsPerSecond = 1000000;

int createTimerfd() {
    const int timerfd =
        ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        std::cerr << "Failed in timerfd_create: " << std::strerror(errno)
                  << '\n';
    }
    return timerfd;
}

int64_t monotonicMicroseconds() {
    struct timespec now {};
    if (::clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        std::cerr << "clock_gettime(CLOCK_MONOTONIC) failed: "
                  << std::strerror(errno) << '\n';
        return 0;
    }
    return static_cast<int64_t>(now.tv_sec) * kMicrosecondsPerSecond +
           now.tv_nsec / 1000;
}

int64_t secondsToMicroseconds(double seconds) {
    if (seconds <= 0.0) return 0;
    const double maxSeconds =
        static_cast<double>(INT64_MAX / kMicrosecondsPerSecond);
    if (seconds >= maxSeconds) return INT64_MAX;
    return std::max<int64_t>(
        1, static_cast<int64_t>(seconds * kMicrosecondsPerSecond));
}

void readTimerfd(int timerfd) {
    uint64_t expirations = 0;
    const ssize_t n = ::read(timerfd, &expirations, sizeof(expirations));
    if (n < 0 && (errno == EAGAIN || errno == EINTR)) return;
    if (n != static_cast<ssize_t>(sizeof(expirations))) {
        std::cerr << "TimerQueue::handleRead() read " << n
                  << " bytes instead of 8\n";
    }
}

}  // namespace

struct TimerQueue::Timer {
    Timer(TimerId timerId, TimerCallback cb, double repeatInterval)
        : id(timerId),
          callback(std::move(cb)),
          interval(secondsToMicroseconds(repeatInterval)),
          repeating(repeatInterval > 0.0) {}

    TimerId id;
    TimerCallback callback;
    int64_t expiration = 0;
    int64_t interval;
    bool repeating;
    bool canceled = false;
};

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop), timerfd_(createTimerfd()), timerfdChannel_(loop, timerfd_) {
    timerfdChannel_.setReadCallback(
        [this](Timestamp) { handleRead(); });
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    if (timerfd_ >= 0) ::close(timerfd_);
}

TimerId TimerQueue::addTimer(TimerCallback cb, double delay, double interval) {
    if (!cb || !std::isfinite(delay) || delay < 0.0 ||
        !std::isfinite(interval) || interval < 0.0) {
        return 0;
    }
    const TimerId timerId = nextTimerId_.fetch_add(1, std::memory_order_relaxed);
    auto timer = std::make_shared<Timer>(timerId, std::move(cb), interval);
    loop_->runInLoop(
        [this, timer = std::move(timer), delay]() { addTimerInLoop(timer, delay); });
    return timerId;
}

void TimerQueue::addTimerInLoop(const std::shared_ptr<Timer>& timer,
                                double delay) {
    loop_->assertInLoopThread();

    const int64_t delayUs = secondsToMicroseconds(delay);
    const int64_t now = monotonicMicroseconds();
    timer->expiration = delayUs > INT64_MAX - now ? INT64_MAX : now + delayUs;

    const bool earliestChanged =
        timers_.empty() || timer->expiration < timers_.begin()->first;
    timers_.emplace(timer->expiration, timer);
    activeTimers_.emplace(timer->id, timer);

    if (earliestChanged) resetTimerfd(timer->expiration);
}

void TimerQueue::cancel(TimerId timerId) {
    if (timerId == 0) return;
    loop_->runInLoop([this, timerId]() { cancelInLoop(timerId); });
}

void TimerQueue::cancelInLoop(TimerId timerId) {
    loop_->assertInLoopThread();
    const auto active = activeTimers_.find(timerId);
    if (active == activeTimers_.end()) return;

    const auto timer = active->second;
    timer->canceled = true;
    const auto entry = timers_.find({timer->expiration, timer});
    const bool earliestChanged = entry != timers_.end() && entry == timers_.begin();
    if (entry != timers_.end()) timers_.erase(entry);
    activeTimers_.erase(active);

    if (earliestChanged) {
        if (timers_.empty()) {
            resetTimerfd(0);
        } else {
            resetTimerfd(timers_.begin()->first);
        }
    }
}

void TimerQueue::handleRead() {
    loop_->assertInLoopThread();
    readTimerfd(timerfd_);

    const int64_t now = monotonicMicroseconds();
    std::vector<std::shared_ptr<Timer>> expired;
    auto end = timers_.begin();
    while (end != timers_.end() && end->first <= now) {
        expired.push_back(end->second);
        ++end;
    }
    timers_.erase(timers_.begin(), end);

    for (const auto& timer : expired) {
        if (!timer->canceled) timer->callback();
    }

    for (const auto& timer : expired) {
        const auto active = activeTimers_.find(timer->id);
        if (active == activeTimers_.end() || timer->canceled) continue;

        if (timer->repeating) {
            const int64_t restartTime = monotonicMicroseconds();
            timer->expiration = timer->interval > INT64_MAX - restartTime
                                    ? INT64_MAX
                                    : restartTime + timer->interval;
            timers_.emplace(timer->expiration, timer);
        } else {
            activeTimers_.erase(active);
        }
    }

    if (!timers_.empty()) resetTimerfd(timers_.begin()->first);
}

void TimerQueue::resetTimerfd(int64_t expiration) {
    struct itimerspec value {};
    if (expiration > 0) {
        value.it_value.tv_sec = static_cast<time_t>(
            expiration / kMicrosecondsPerSecond);
        value.it_value.tv_nsec = static_cast<long>(
            (expiration % kMicrosecondsPerSecond) * 1000);
    }

    if (::timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &value, nullptr) < 0) {
        std::cerr << "timerfd_settime failed: " << std::strerror(errno) << '\n';
    }
}
