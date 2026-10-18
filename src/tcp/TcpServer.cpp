#include "TcpServer.h"

#include <cstdio>  // snprintf
#include <functional>
#include <string>

#include "../base/Logger.h"
#include "../net/EventLoop.h"
#include "../pool/EventLoopThreadPool.h"
#include "Acceptor.h"
#include "TcpConnection.h"

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if (loop == nullptr) {
        LOG_FATAL << "MainLoop is null!";
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr,
                     const std::string &nameArg)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr)),
      threadPool_(new EventLoopThreadPool(loop, nameArg)),
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(1),
      started_(0) {

    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,
                                                  this, std::placeholders::_1,
                                                  std::placeholders::_2));
}

TcpServer::~TcpServer() {
    LOG_INFO << "TcpServer::~TcpServer [" << name_ << "] destructing";

    for (auto &item : connections_) {

        TcpConnectionPtr conn(item.second);

        item.second.reset();

        // Use-After-Free
        conn->setConnectionCallback([](const TcpConnectionPtr &) {});
        conn->setMessageCallback(
            [](const TcpConnectionPtr &, Buffer *, Timestamp) {});

        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {

    if (started_++ == 0) {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// ---------------------------------------------------------

// ---------------------------------------------------------
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    loop_->assertInLoopThread();

    EventLoop *ioLoop = threadPool_->getNextLoop();

    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO << "TcpServer::newConnection [" << name_ << "] - new connection ["
             << connName << "] from " << peerAddr.toIpPort();

    InetAddress localAddr(peerAddr);

    TcpConnectionPtr conn(
        new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));

    connections_[connName] = conn;

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

// ---------------------------------------------------------

// ---------------------------------------------------------

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {

    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

// ---------------------------------------------------------

// ---------------------------------------------------------

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    loop_->assertInLoopThread();

    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
             << "] - connection " << conn->name();

    size_t n = connections_.erase(conn->name());
    (void)n;

    EventLoop *ioLoop = conn->getLoop();

    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}