#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "../net/EventLoop.h"
#include "../net/InetAddress.h"
#include "../pool/EventLoopThreadPool.h"
#include "Acceptor.h"
#include "Callbacks.h"
#include "TcpConnection.h"

// Accepts new connections on the base loop and assigns them to worker loops.
class TcpServer : noncopyable {
   public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    TcpServer(EventLoop *loop, const InetAddress &listenAddr,
              const std::string &nameArg);
    ~TcpServer();

    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback &cb) {
        threadInitCallback_ = cb;
    }

    void start();

    void setConnectionCallback(const ConnectionCallback &cb) {
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback &cb) {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
        writeCompleteCallback_ = cb;
    }

   private:

    void newConnection(int sockfd, const InetAddress &peerAddr);

    using ConnectionMap = std::map<std::string, TcpConnectionPtr>;

    EventLoop *loop_;
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;

    std::atomic_int started_;
    int nextConnId_;
    std::shared_ptr<ConnectionMap> connections_;
};
