#include "TimerQueue.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "EventLoop.h"

// --- 底层 timerfd 辅助函数 ---
int createTimerfd() {
    // 🔥 核心考点：使用 CLOCK_MONOTONIC 单调时钟，不受系统修改时间影响
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        std::cerr << "Failed in timerfd_create" << "\n";
    }
    return timerfd;
}

void readTimerfd(int timerfd, Timestamp now) {
    uint64_t howmany;
    // 当 timerfd 触发读事件时，必须把它读出来，否则 epoll 会一直不断触发
    // (LT模式)
    ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));
    if (n != sizeof(howmany)) {
        std::cerr << "TimerQueue::handleRead() reads " << n
                  << " bytes instead of 8" << "\n";
    }
}
// -----------------------------

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop), timerfd_(createTimerfd()), timerfdChannel_(loop, timerfd_) {
    // 将 handleRead 绑定到 Channel 的读事件上
    timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
    // 向 EventLoop 注册这个 Channel（也就是把 timerfd 挂到 epoll 树上）
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue() {
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
    // 清理遗留的 Timer 内存
    for (const auto& entry : timers_) {
        delete entry.second;
    }
}

void TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval) {
    Timer* timer = new Timer(std::move(cb), when, interval);

    // TODO: 为了线程安全，这里未来需要用 loop_->runInLoop 包装
    // 目前为了让你能顺利编译运行，直接在当前线程插入
    // （等你以后给服务器加上了多线程 EventLoopThreadPool，再把这里改成
    // loop_->runInLoop）
    bool earliestChanged = insert(timer);

    // 如果新插入的定时器是最早到期的，我们需要立刻更新底层 timerfd 的报警时间
    if (earliestChanged) {
        resetTimerfd(timerfd_, timer->expiration());
    }
}
void TimerQueue::handleRead() {
    Timestamp now(Timestamp::now());  // 获取当前时间
    readTimerfd(timerfd_, now);       // 清除 timerfd 的可读状态

    // 1. 找出所有到期的定时器
    std::vector<Timer*> expired = getExpired(now);

    // 2. 依次执行它们的回调函数
    for (Timer* timer : expired) {
        if (timer->isCanceled()) {
            delete timer;
            continue;
        }
        timer->run();
    }

    // TODO: 3.处理重复任务与清理内存
    for (Timer* timer : expired) {
        if (timer->repeat()) {
            timer->restart(now);  // 重新计算下次时间
            insert(timer);        // 重新塞回红黑树
        } else {
            delete timer;  // 一次性任务，执行完立刻销毁，防止内存泄漏！
        }
    }

    // TODO: 4. 关键收尾：如果有剩余定时器，把最靠前的那个时间设置给 Linux 底层
    if (!timers_.empty()) {
        Timestamp nextExpire = timers_.begin()->second->expiration();
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer* timer) {
    bool earliestChanged = false;
    Timestamp when = timer->expiration();

    // 如果集合为空，或者新定时器的时间比集合里最早的还要早
    if (timers_.empty() || when < timers_.begin()->first) {
        earliestChanged = true;
    }

    timers_.insert({when, timer});
    return earliestChanged;
}

std::vector<Timer*> TimerQueue::getExpired(Timestamp now) {
    std::vector<Timer*> expired;
    // 技巧：创建一个“现在”的 dummy 节点，用来在红黑树里做二分查找
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    // lower_bound 返回第一个 未到期 的迭代器
    auto end = timers_.lower_bound(sentry);

    // 把从 begin 到 end 之前（已到期）的都捞出来
    for (auto it = timers_.begin(); it != end; ++it) {
        expired.push_back(it->second);
    }

    // 从红黑树里删掉这些已经到期的任务
    timers_.erase(timers_.begin(), end);

    return expired;
}

void TimerQueue::resetTimerfd(int timerfd, Timestamp expiration) {
    // TODO: 使用 timerfd_settime 函数设置 Linux 底层报警器
    // 需要把 expiration (绝对时间) 转换成 struct itimerspec 给系统

    struct itimerspec newValue;
    struct itimerspec oldValue;
    bzero(&newValue, sizeof newValue);
    bzero(&oldValue, sizeof oldValue);

    // 计算还要多久触发 = 目标时间 - 现在的时间
    int64_t microSecondsDiff = expiration.microSecondsSinceEpoch() -
                               Timestamp::now().microSecondsSinceEpoch();

    // 防御性编程：哪怕已经过期了，也强行让它 100 微秒后触发
    // （如果设为 0，Linux 会以为你要关闭定时器）
    if (microSecondsDiff < 100) {
        microSecondsDiff = 100;
    }

    // 把微秒转换为秒和纳秒，填入 itimerspec 结构体
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microSecondsDiff /
                                    Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
        (microSecondsDiff % Timestamp::kMicroSecondsPerSecond) * 1000);
    newValue.it_value = ts;

    // 调用系统 API 设置底层定时器
    if (::timerfd_settime(timerfd, 0, &newValue, &oldValue) < 0) {
        std::cerr << "timerfd_settime error!" << "\n";
    }
}