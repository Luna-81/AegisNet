#pragma once
#include <functional>

#include "../base/noncopyable.h"
#include "../net/Channel.h"
#include "../net/Socket.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
   public:
    // 定义回调：当有新连接时，Acceptor 把 (connfd, peerAddr) 传给 TcpServer
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
    void handleRead(Timestamp receiveTime);  // 内部处理读事件（即调用 accept）

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
};