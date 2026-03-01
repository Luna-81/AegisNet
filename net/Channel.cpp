#include "Channel.h"

#include <sys/epoll.h>

#include <functional>
#include <mutex>
#include <vector>

#include "EventLoop.h"

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      inEpoll_(false),
      tied_(false) {}

Channel::~Channel() {}

// 绑定对象，通常是 TcpConnection
void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}

// 内部更新函数，通知 EventLoop 改变 Epoll 监听行为
void Channel::update() { loop_->updateChannel(this); }

// 事件处理入口
void Channel::handleEvent(Timestamp receiveTime) {
    if (tied_) {
        // 尝试把弱引用提升为强引用
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
        // 如果提升失败，说明 TcpConnection 已经析构了，不再执行回调
    } else {
        handleEventWithGuard(receiveTime);
    }
}

// 具体的事件分发逻辑
void Channel::handleEventWithGuard(Timestamp receiveTime) {
    // 1. 异常事件
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) closeCallback_();
    }

    // 2. 错误事件
    if (revents_ & EPOLLERR) {
        if (errorCallback_) errorCallback_();
    }

    // 3. 可读事件 (包含普通数据、紧急数据、对端半关闭)
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_) readCallback_(receiveTime);
    }

    // 4. 可写事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}

void Channel::remove() {
    disableAll();  // 先在 Epoll 中设为 0 事件 (MOD)

    // 🔥 2. 必须通知 EventLoop 将自己从监听列表和 map 中彻底删除 (DEL)
    // 假设你的 EventLoop 有 removeChannel 方法
    loop_->removeChannel(this);
}