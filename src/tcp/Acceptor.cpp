#include "Acceptor.h"

#include <unistd.h>

#include <iostream>

#include "../net/InetAddress.h"

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      acceptSocket_(),
      acceptChannel_(loop, acceptSocket_.getFd()),
      listening_(false) {

    acceptSocket_.setNonBlocking();

    acceptSocket_.setReuseAddr(true);

    acceptSocket_.bindAddress(listenAddr);

    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this, std::placeholders::_1));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen() {
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
    std::cout << "✅ Acceptor::listen() called. Fd=" << acceptSocket_.getFd()
              << " is now monitored by Epoll." << "\n";
}

void Acceptor::handleRead(Timestamp receiveTime) {
    InetAddress peerAddr;

    while (true) {
        int connfd = acceptSocket_.accept(&peerAddr);
        if (connfd >= 0) {
            if (newConnectionCallback_) {
                newConnectionCallback_(connfd, peerAddr);
            } else {
                ::close(connfd);
            }
        } else {

            break;
        }
    }
}