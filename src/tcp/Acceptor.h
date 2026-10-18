#pragma once
#include <functional>

#include "../base/noncopyable.h"
#include "../net/Channel.h"
#include "../net/Socket.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
   public:

    using NewConnectionCallback =
        std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

    bool listening() const { return listening_; }
    void listen();

   private:
    void handleRead(Timestamp receiveTime);

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
};