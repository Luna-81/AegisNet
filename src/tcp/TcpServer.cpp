#include "TcpServer.h"

#include <cstdio>  // snprintf
#include <functional>
#include <string>

#include "../base/Logger.h"
#include "../net/EventLoop.h"
#include "../pool/EventLoopThreadPool.h"
#include "Acceptor.h"
#include "TcpConnection.h"

// 检查 EventLoop 是否为空的辅助函数
static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if (loop == nullptr) {
        LOG_FATAL << "MainLoop is null!";  // LOG_FATAL 会自动 exit(-1)
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
    // 当 Acceptor 有新用户连接时，执行 TcpServer::newConnection
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,
                                                  this, std::placeholders::_1,
                                                  std::placeholders::_2));
}

// day17 全改
TcpServer::~TcpServer() {
    LOG_INFO << "TcpServer::~TcpServer [" << name_ << "] destructing";

    for (auto &item : connections_) {
        // item.first 是 connName, item.second 是 TcpConnectionPtr
        TcpConnectionPtr conn(item.second);

        // 1. 必须打开！释放 Map 持有的智能指针，打破与 TcpConnection 的循环引用
        item.second.reset();

        // 2. 核心防御：清空业务回调！
        // 防止子线程在 connectDestroyed 中回调到已经被销毁的 EchoServer 导致
        // Use-After-Free
        conn->setConnectionCallback([](const TcpConnectionPtr &) {});
        conn->setMessageCallback(
            [](const TcpConnectionPtr &, Buffer *, Timestamp) {});

        // 3. 销毁连接
        // 必须让连接所属的 IO 线程去执行 connectDestroyed
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads) {
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
    // 防止被多次调用
    if (started_++ == 0) {
        threadPool_->start(threadInitCallback_);  // 启动线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// ---------------------------------------------------------
// 🔥 核心：新连接到来
// ---------------------------------------------------------
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    loop_->assertInLoopThread();  // 必须在主线程运行

    // 1. 轮询算法选择一个 SubLoop (IO 线程)
    EventLoop *ioLoop = threadPool_->getNextLoop();

    // 2. 构造连接名称
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO << "TcpServer::newConnection [" << name_ << "] - new connection ["
             << connName << "] from " << peerAddr.toIpPort();

    // 3. 获取本地地址 (本机IP:Port)
    // 暂且用 peerAddr 占位，避免编译错误，实际应使用 getsockname
    InetAddress localAddr(peerAddr);

    // 4. 创建 TcpConnection 对象
    TcpConnectionPtr conn(
        new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));

    // 5. 将连接加入 Map
    connections_[connName] = conn;

    // 6. 设置用户回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 7. 【关键】设置关闭回调
    // 当 TcpConnection 想要关闭时，它会回调 TcpServer::removeConnection
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 8. 在 IO 线程中执行连接建立完成
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

// ---------------------------------------------------------
// 🔥 核心：断开连接 (第一步)
// ---------------------------------------------------------
// 此时我们在 SubLoop (IO线程) 中
void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
    // 切换到 MainLoop 执行，因为 connections_ Map 属于 MainLoop
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

// ---------------------------------------------------------
// 🔥 核心：断开连接 (第二步)
// ---------------------------------------------------------
// 此时我们在 MainLoop 中
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
    loop_->assertInLoopThread();

    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
             << "] - connection " << conn->name();

    // 1. 从 Map 中删除
    // 此时 Map 中的 shared_ptr 被销毁，引用计数 -1
    // 但是 conn 这个参数 (shared_ptr) 依然持有对象，所以对象还没死
    size_t n = connections_.erase(conn->name());
    (void)n;

    // 2. 拿到连接所属的 Loop
    EventLoop *ioLoop = conn->getLoop();

    // 3. 切回 IO 线程做最后的清理
    // queueInLoop 确保在 IO 线程处理完当前所有事件后，才执行销毁
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}