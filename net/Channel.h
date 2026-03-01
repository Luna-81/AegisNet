#pragma once
#include <sys/epoll.h>

#include <functional>
#include <memory>

#include "../base/Timestamp.h"
class EventLoop;

/**
 * Channel 理解为“文件描述符的保姆”
 * 它负责一个 fd 的事件注册、事件分发。
 */
class Channel {
   public:
    using EventCallback = std::function<void()>;
    // 👇 新增这一行：专门给读事件用的回调类型，带时间戳参数
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // 核心：处理由 Epoll 分发过来的事件
    void handleEvent(Timestamp receiveTime);

    // 设置各类回调
    void setReadCallback(ReadEventCallback cb) {
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 【Day 6 重点】生命周期绑定：防止 TcpConnection 在执行回调时被析构
    void tie(const std::shared_ptr<void> &);

    // 事件控制
    void enableReading() {
        events_ |= EPOLLIN;
        update();
    }
    void disableReading() {
        events_ &= ~EPOLLIN;
        update();
    }
    void enableWriting() {
        events_ |= EPOLLOUT | EPOLLET;
        update();
    }
    void disableWriting() {
        events_ &= ~EPOLLOUT;
        update();
    }
    void disableAll() {
        events_ = 0;
        update();
    }

    // 状态查询
    int getFd() const { return fd_; }
    uint32_t getEvents() const { return events_; }
    void setRevents(uint32_t rev) { revents_ = rev; }
    bool getInEpoll() const { return inEpoll_; }
    void setInEpoll(bool in = true) { inEpoll_ = in; }

    bool isWriting() const { return events_ & EPOLLOUT; }
    bool isReading() const { return events_ & EPOLLIN; }

    // day7 添加这个声明
    void remove();

   private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);  // 真正执行回调的函数

    EventLoop *loop_;
    const int fd_;
    uint32_t events_;   // 注册的事件
    uint32_t revents_;  // 实际发生的事件
    bool inEpoll_;

    // 生命周期保护相关
    std::weak_ptr<void> tie_;
    bool tied_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};