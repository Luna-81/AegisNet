#pragma once

#include <atomic>
#include <cstdlib>  // 🔥 abort() 需要
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "../base/CurrentThread.h"
#include "../base/noncopyable.h"
#include "TimerQueue.h"

class Channel;
class Epoll;  // 前置声明，减少头文件依赖

class EventLoop : noncopyable {
   public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();

    // 退出事件循环
    void quit();

    // 唤醒 Loop 所在线程 (通过 eventfd 写数据)
    void wakeup();

    // 更新 Channel (调用 Epoll)
    void updateChannel(Channel* channel);

    // day10 移除 Channel
    void removeChannel(Channel* channel);

    // 判断当前是否在 Loop 线程
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

    // 断言
    void assertInLoopThread() {
        if (!isInLoopThread()) {
            abort();
        }
    }

    // === 核心任务队列接口 ===

    // 如果在当前线程，直接执行；否则放入队列
    void runInLoop(Functor cb);

    // 把任务放入队列，并唤醒 Loop 线程
    void queueInLoop(Functor cb);

    //day20 新增
    void runEvery(double interval, std::function<void()> cb);

   private:
    // eventfd 的读回调
    void handleRead();

    // 执行队列中的所有任务
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;  // 是否正在循环
    std::atomic_bool quit_;     // 是否退出

    const pid_t threadId_;  // 记录创建 Loop 时的线程 ID (使用 int 类型)

    std::unique_ptr<Epoll> ep_;  // IO 复用器 (Epoll)

    int wakeupFd_;  // eventfd，用于跨线程唤醒
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;  // 当前活跃的 Channel 列表

    std::atomic_bool callingPendingFunctors_;  // 标识是否正在执行回调
    std::vector<Functor> pendingFunctors_;     // 任务队列
    std::mutex mutex_;                         // 保护任务队列的锁
    // 关键：声明定时器队列
    std::unique_ptr<TimerQueue> timerQueue_;

};