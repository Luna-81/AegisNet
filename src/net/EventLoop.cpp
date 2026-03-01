#include "EventLoop.h"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>

#include "../base/Logger.h"  // 建议加上日志
#include "../base/Timestamp.h"
#include "Channel.h"
#include "Epoll.h"
#include "TimerQueue.h"

// 防止在该编译单元外被看到
namespace {
// 创建 eventfd
int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL << "Failed in eventfd";  // 如果没有 LOG_FATAL，用 abort()
    }
    return evtfd;
}
}  // namespace

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      threadId_(CurrentThread::tid()),  // 🔥 获取当前线程整数 ID
      ep_(new Epoll()),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      callingPendingFunctors_(false),timerQueue_(new TimerQueue(this)) {
    // 设置 wakeupFd 的读回调
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 开启读监听 (EPOLLIN)
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

        // 1. Epoll 等待事件 (Timeout = -1 阻塞, 或者 10000ms)
        // ep_->poll 会把活跃的 channel 填入 activeChannels_
        ep_->poll(activeChannels_);

        // day14 🚀 核心优化 B：Epoll 刚醒来，立刻“查一次手表”！
        // 这一轮循环中的所有事件，都将共享这同一个时间戳，省去了成千上万次系统调用！
        Timestamp pollReturnTime = Timestamp::now();

        // 2. 处理 IO 事件
        for (Channel* channel : activeChannels_) {
            channel->handleEvent(pollReturnTime);
        }

        // 3. 处理任务队列 (跨线程投递的任务)
        doPendingFunctors();
    }

    LOG_INFO << "EventLoop " << this << " stop looping";
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    // 如果在其他线程调用 quit，必须唤醒 Loop 才能让它从 poll 中返回并退出 while
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();  // 当前线程直接执行
    } else {
        queueInLoop(std::move(cb));  // 放入队列
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }

    // 唤醒逻辑：
    // 1. 如果不是当前线程 -> 必须唤醒
    // 2. 如果是当前线程，但正在执行 pendingFunctors -> 必须唤醒
    //    (防止在回调里又 queueInLoop，导致新任务要等下一轮 poll 才能执行)
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
        // 核心优化：Swap
        // 只需要加锁这一瞬间，把 pendingFunctors_ 倒换到局部变量 functors 中
        // 这样执行回调时就不需要持锁了，避免了死锁风险，也提升了并发度
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);

        // day14 🚀 加上这一句！刚才 swap 把容量也换走了，这里要提前补足空车位
        // 避免高并发下其他线程往 pendingFunctors_ 里塞任务时频繁触发内存分配！
        if (pendingFunctors_.capacity() < 128) {
            pendingFunctors_.reserve(128);
        }
    }

    for (const auto& functor : functors) {
        functor();
    }

    callingPendingFunctors_ = false;
}

// 在 EventLoop 类定义中
void EventLoop::runEvery(double interval, std::function<void()> cb) {
    // 这里的 timerQueue_ 是你昨天（Day 19）封装的组件
    timerQueue_->addTimer(std::move(cb), Timestamp::now(), interval);
}

