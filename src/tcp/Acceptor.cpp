#include "Acceptor.h"

#include <unistd.h>

#include <iostream>

#include "../net/InetAddress.h"

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      acceptSocket_(),
      acceptChannel_(loop, acceptSocket_.getFd()),
      listening_(false) {
    // 1. 设置非阻塞（最关键！防止主线程卡死在 accept）
    acceptSocket_.setNonBlocking();

    // 2. 设置地址复用
    acceptSocket_.setReuseAddr(true);

    // 3. 绑定端口
    acceptSocket_.bindAddress(listenAddr);

    // 4. 设置监听回调：当 listenfd 有读事件（新连接）时，执行 handleRead
    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this, std::placeholders::_1));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen() {
    listening_ = true;
    acceptSocket_.listen();  // 这里现在会检查错误了
    acceptChannel_.enableReading();
    std::cout << "✅ Acceptor::listen() called. Fd=" << acceptSocket_.getFd()
              << " is now monitored by Epoll." << "\n";
}

// 👇 加上 Timestamp receiveTime 参数
void Acceptor::handleRead(Timestamp receiveTime) {
    InetAddress peerAddr;
    // 循环 accept 直到队列为空
    while (true) {
        int connfd = acceptSocket_.accept(&peerAddr);
        if (connfd >= 0) {
            if (newConnectionCallback_) {
                newConnectionCallback_(connfd, peerAddr);
            } else {
                ::close(connfd);
            }
        } else {
            // 没有更多连接了 (EAGAIN)
            break;
        }
    }
}