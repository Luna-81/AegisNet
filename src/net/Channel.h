#pragma once
#include <sys/epoll.h>

#include <functional>
#include <memory>

#include "../base/Timestamp.h"
class EventLoop;

// Associates one file descriptor with its epoll events and callbacks.
class Channel {
   public:
    using EventCallback = std::function<void()>;

    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    void handleEvent(Timestamp receiveTime);

    void setReadCallback(ReadEventCallback cb) {
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    void tie(const std::shared_ptr<void> &);

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

    int getFd() const { return fd_; }
    uint32_t getEvents() const { return events_; }
    void setRevents(uint32_t rev) { revents_ = rev; }
    bool getInEpoll() const { return inEpoll_; }
    void setInEpoll(bool in = true) { inEpoll_ = in; }

    bool isWriting() const { return events_ & EPOLLOUT; }
    bool isReading() const { return events_ & EPOLLIN; }

    void remove();

   private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    EventLoop *loop_;
    const int fd_;
    uint32_t events_;
    uint32_t revents_;
    bool inEpoll_;

    std::weak_ptr<void> tie_;
    bool tied_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
