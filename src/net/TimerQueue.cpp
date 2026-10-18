#include "TimerQueue.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "EventLoop.h"

int createTimerfd() {

    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        std::cerr << "Failed in timerfd_create" << "\n";
    }
    return timerfd;
}

void readTimerfd(int timerfd, Timestamp now) {
    uint64_t howmany;

    ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));
    if (n != sizeof(howmany)) {
        std::cerr << "TimerQueue::handleRead() reads " << n
                  << " bytes instead of 8" << "\n";
    }
}
// -----------------------------

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop), timerfd_(createTimerfd()), timerfdChannel_(loop, timerfd_) {

    timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));

    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);

    for (const auto& entry : timers_) {
        delete entry.second;
    }
}

void TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval) {
    Timer* timer = new Timer(std::move(cb), when, interval);

    // loop_->runInLoop）
    bool earliestChanged = insert(timer);

    if (earliestChanged) {
        resetTimerfd(timerfd_, timer->expiration());
    }
}
void TimerQueue::handleRead() {
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);

    std::vector<Timer*> expired = getExpired(now);

    for (Timer* timer : expired) {
        if (timer->isCanceled()) {
            delete timer;
            continue;
        }
        timer->run();
    }

    for (Timer* timer : expired) {
        if (timer->repeat()) {
            timer->restart(now);
            insert(timer);
        } else {
            delete timer;
        }
    }

    if (!timers_.empty()) {
        Timestamp nextExpire = timers_.begin()->second->expiration();
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer* timer) {
    bool earliestChanged = false;
    Timestamp when = timer->expiration();

    if (timers_.empty() || when < timers_.begin()->first) {
        earliestChanged = true;
    }

    timers_.insert({when, timer});
    return earliestChanged;
}

std::vector<Timer*> TimerQueue::getExpired(Timestamp now) {
    std::vector<Timer*> expired;

    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));

    auto end = timers_.lower_bound(sentry);

    for (auto it = timers_.begin(); it != end; ++it) {
        expired.push_back(it->second);
    }

    timers_.erase(timers_.begin(), end);

    return expired;
}

void TimerQueue::resetTimerfd(int timerfd, Timestamp expiration) {

    struct itimerspec newValue;
    struct itimerspec oldValue;
    bzero(&newValue, sizeof newValue);
    bzero(&oldValue, sizeof oldValue);

    int64_t microSecondsDiff = expiration.microSecondsSinceEpoch() -
                               Timestamp::now().microSecondsSinceEpoch();

    if (microSecondsDiff < 100) {
        microSecondsDiff = 100;
    }

    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microSecondsDiff /
                                    Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
        (microSecondsDiff % Timestamp::kMicroSecondsPerSecond) * 1000);
    newValue.it_value = ts;

    if (::timerfd_settime(timerfd, 0, &newValue, &oldValue) < 0) {
        std::cerr << "timerfd_settime error!" << "\n";
    }
}