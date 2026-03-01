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

class TcpServer : noncopyable {
   public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    TcpServer(EventLoop *loop, const InetAddress &listenAddr,
              const std::string &nameArg);
    ~TcpServer();

    // 设置线程数量 (0 = 单线程, >0 = 多线程)
    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback &cb) {
        threadInitCallback_ = cb;
    }

    // 开启服务器监听
    void start();

    // === 用户回调设置 ===
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
    // === 内部处理函数 ===

    // Acceptor 接受新连接后，调用此函数
    void newConnection(int sockfd, const InetAddress &peerAddr);

    // TcpConnection 断开时，调用此函数 (运行在 IO 线程)
    void removeConnection(const TcpConnectionPtr &conn);

    // removeConnection 跳转到主线程执行的实际逻辑
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    // === 成员变量 ===

    // Key: 连接名称, Value: 连接的智能指针
    using ConnectionMap = std::map<std::string, TcpConnectionPtr>;

    EventLoop *loop_;  // 主 Reactor (只负责 Accept)
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;               // 负责监听 socket
    std::unique_ptr<EventLoopThreadPool> threadPool_;  // 线程池

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;

    std::atomic_int started_;
    int nextConnId_;             // 下一个连接 ID
    ConnectionMap connections_;  // 保存所有存活连接
};