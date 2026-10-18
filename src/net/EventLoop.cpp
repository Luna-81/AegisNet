#include "EventLoop.h"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>

#include "../base/Logger.h"
#include "../base/Timestamp.h"
#include "Channel.h"
#include "Epoll.h"
#include "TimerQueue.h"

namespace {

int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL << "Failed in eventfd";
    }
    return evtfd;
}
}  // namespace

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      threadId_(CurrentThread::tid()),
      ep_(new Epoll()),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      callingPendingFunctors_(false),timerQueue_(new TimerQueue(this)) {

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));

    wakeupChannel_->enableReading();

    LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
}

void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    LOG_INFO << "EventLoop " << this << " start looping";

    while (!quit_) {
        activeChannels_.clear();

        ep_->poll(activeChannels_);

        Timestamp pollReturnTime = Timestamp::now();

        for (Channel* channel : activeChannels_) {
            channel->handleEvent(pollReturnTime);
        }

        doPendingFunctors();
    }

    LOG_INFO << "EventLoop " << this << " stop looping";
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;

    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR << "EventLoop::handleRead() reads " << n
                  << " bytes instead of 8";
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR << "EventLoop::wakeup() writes " << n
                  << " bytes instead of 8";
    }
}

void EventLoop::updateChannel(Channel* channel) { ep_->updateChannel(channel); }

void EventLoop::removeChannel(Channel* channel) { ep_->removeChannel(channel); }

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {

        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);

        if (pendingFunctors_.capacity() < 128) {
            pendingFunctors_.reserve(128);
        }
    }

    for (const auto& functor : functors) {
        functor();
    }

    callingPendingFunctors_ = false;
}

TimerId EventLoop::runAfter(double delay, std::function<void()> cb) {
    return timerQueue_->addTimer(std::move(cb), delay, 0.0);
}

TimerId EventLoop::runEvery(double interval, std::function<void()> cb) {
    if (!std::isfinite(interval) || interval <= 0.0) return 0;
    return timerQueue_->addTimer(std::move(cb), interval, interval);
}

void EventLoop::cancel(TimerId timerId) {
    timerQueue_->cancel(timerId);
}
