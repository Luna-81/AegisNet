#pragma once

#include <functional>
#include <set>
#include <vector>

#include "../base/Timestamp.h"  // 假设你有 Timestamp 类，没有的话可以用 uint64_t
#include "Channel.h"

class EventLoop;

// 1. 单个定时器的封装 (为了简单，直接写在同一个头文件或新建 Timer.h)
class Timer {
   public:
    using TimerCallback = std::function<void()>;
    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(std::move(cb)),
          expiration_(when),
          interval_(interval),
          repeat_(interval > 0.0),
          canceled_(false) {}  // 💡 初始化时默认是 false (未取消)
    void run() const {
        if (!canceled_) callback_();
    }  // 要做判断
    Timestamp expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }
    // 👇 就是这里！把原来的 void restart(Timestamp now); 替换成这段完整的代码
    void restart(Timestamp now) {
        if (repeat_) {
            // 如果是重复定时器，下次时间 = 现在的时间 + 间隔时间
            expiration_ = addTime(now, interval_);
        } else {
            // 如果不是，置为无效时间
            expiration_ = Timestamp();
        }
    }
    // 👇 新增的三个核心代码
    void cancel() { canceled_ = true; }            // 标记为已取消
    bool isCanceled() const { return canceled_; }  // 查询是否被取消

   private:
    const TimerCallback callback_;
    Timestamp expiration_;
    const double interval_;
    const bool repeat_;

    bool canceled_;  // 💡 新增的标志位
};

// 2. 定时器队列管理
class TimerQueue {
   public:
    using TimerCallback = std::function<void()>;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // 暴露给外部的接口：添加定时器
    void addTimer(TimerCallback cb, Timestamp when, double interval);

   private:
    // 绑定到 Channel 上的读事件回调
    void handleRead();

    // 获取所有已经到期的定时器
    std::vector<Timer*> getExpired(Timestamp now);

    // 重新设置 timerfd 的报警时间
    void resetTimerfd(int timerfd, Timestamp expiration);

    // 内部插入操作
    bool insert(Timer* timer);

    EventLoop* loop_;
    const int timerfd_;       // Linux 提供的定时器文件描述符
    Channel timerfdChannel_;  // 用 Channel 包装 timerfd，交给 EventLoop 监听

    // 核心数据结构：按到期时间排序的红黑树
    // 用 pair 是因为可能有多个定时器在同一微秒到期，加上 Timer* 地址保证唯一性
    using Entry = std::pair<Timestamp, Timer*>;
    std::set<Entry> timers_;
};
