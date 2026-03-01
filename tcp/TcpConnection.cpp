#include "TcpConnection.h"

#include <errno.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <functional>

#include "../base/Logger.h"
#include "../net/Channel.h"
#include "../net/EventLoop.h"
#include "../net/Socket.h"

// 用于检查 Loop 指针是否为空的辅助函数
static EventLoop *CheckLoopNotNull(EventLoop *loop) {
    if (loop == nullptr) {
        LOG_FATAL << "TcpConnection Loop is null!";
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg,
                             int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr) {
    // 通道回调绑定
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    // 使用 LOG_INFO 或 LOG_DEBUG
    LOG_INFO << "TcpConnection::ctor[" << name_ << "] at " << this
             << " fd=" << sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG_INFO << "TcpConnection::dtor[" << name_ << "] at " << this
             << " fd=" << channel_->getFd() << " state=" << state_;
}

int TcpConnection::getFd() const { return channel_->getFd(); }

// === 第一阶段：连接建立 ===

void TcpConnection::connectEstablished() {
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);

    // 绑定生命周期
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 注册 EPOLLIN

    if (connectionCallback_) {
        connectionCallback_(shared_from_this());
    }
}

void TcpConnection::handleRead(Timestamp receiveTime) {
    loop_->assertInLoopThread();
    int savedErrno = 0;

    // 从 fd 读取数据到 InputBuffer
    ssize_t n = inputBuffer_.readFd(channel_->getFd(), &savedErrno);

    // 🔥 加上 likely：这是最高频的路径，告诉 CPU 全力冲刺这块代码！
    if (likely(n > 0)) {
        // 读到数据，回调用户
        if (messageCallback_) {
            messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        }
    } else if (n == 0) {
        // 读到 0，代表对端关闭
        handleClose();
    } else {
        // 走到这里 n < 0，发生错误
        errno = savedErrno;
        LOG_ERROR << "TcpConnection::handleRead error";
        handleError();
    }
}
// day16 大改 handleWrite
void TcpConnection::handleWrite() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        LOG_INFO << "Connection fd=" << channel_->getFd()
                 << " is down, no more writing";
        return;
    }

    // ==========================================
    // 阶段 1：先发送 OutputBuffer 里的数据 (HTTP 头部)
    // ==========================================
    if (outputBuffer_.readableBytes() > 0) {
        ssize_t n = ::write(channel_->getFd(), outputBuffer_.peek(),
                            outputBuffer_.readableBytes());
        if (n > 0) {
            outputBuffer_.retrieve(n);
        } else if (n < 0 && errno != EAGAIN) {
            LOG_ERROR << "TcpConnection::handleWrite write outputBuffer error";
            return;  // 发生致命错误，等待 handleError 处理
        }
    }

    // ==========================================
    // 阶段 2：如果头部发完了，且有文件需要发，启动零拷贝！
    // ==========================================
    if (outputBuffer_.readableBytes() == 0 && fileFd_ >= 0) {
        // 注意：&fileOffset_ 传进去后，内核会自动帮你推进这个偏移量！
        ssize_t n = ::sendfile(channel_->getFd(), fileFd_, &fileOffset_,
                               fileSize_ - fileOffset_);

        if (n > 0) {
            // 成功发送了 n 字节，继续等下一次循环或直接进入阶段 3
        } else if (n < 0) {
            if (errno == EAGAIN) {
                // 🚗 核心精髓：网卡堵车了！直接 return，保留 fileFd_ 和
                // fileOffset_。 因为 channel_->isWriting() 还是 true，Epoll
                // 等会儿会再次叫醒我们接着发！
                return;
            } else {
                LOG_ERROR << "TcpConnection::handleWrite sendfile error";
                ::close(fileFd_);
                fileFd_ = -1;
                return;
            }
        }
    }

    // ==========================================
    // 阶段 3：检查是否所有任务都大功告成
    // ==========================================
    // 条件：Buffer 彻底空了，并且 (没有文件要发 或者 文件也发到头了)
    if (outputBuffer_.readableBytes() == 0 &&
        (fileFd_ < 0 || fileOffset_ == fileSize_)) {
        // 1. 停用 EPOLLOUT，防止 CPU 疯狂空转
        channel_->disableWriting();

        // 2. 擦屁股：关闭文件句柄
        if (fileFd_ >= 0) {
            ::close(fileFd_);
            fileFd_ = -1;
            LOG_INFO << "✅ 静态文件发送完毕，零拷贝大成功！";
        }

        // 3. 触发低水位回调
        if (writeCompleteCallback_) {
            loop_->queueInLoop(
                std::bind(writeCompleteCallback_, shared_from_this()));
        }

        // 4. 处理挥手逻辑 (如果是 HTTP/1.0 或者 Connection: close)
        if (state_ == kDisconnecting) {
            shutdownInLoop();
        }
    }
}

void TcpConnection::send(const std::string &buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf);
        } else {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf));
        }
    }
}

void TcpConnection::sendInLoop(const std::string &message) {
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = message.size();
    bool faultError = false;

    if (state_ == kDisconnected) {
        LOG_ERROR << "disconnected, give up writing";
        return;
    }

    // 1. 尝试直接发送
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->getFd(), message.data(), message.size());
        if (nwrote >= 0) {
            remaining = message.size() - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    faultError = true;
                }
            }
        }
    }

    // 2. 如果没写完，追加到 Buffer
    if (!faultError && remaining > 0) {
        outputBuffer_.append(message.data() + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

// day16 新增
void TcpConnection::sendFile(int fd, size_t fileSize) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendFileInLoop(fd, fileSize);
        } else {
            loop_->runInLoop(
                std::bind(&TcpConnection::sendFileInLoop, this, fd, fileSize));
        }
    }
}
// day16 新增
void TcpConnection::sendFileInLoop(int fd, size_t fileSize) {
    loop_->assertInLoopThread();
    if (state_ == kDisconnected) {
        LOG_ERROR << "disconnected, give up sending file";
        ::close(fd);  // 既然不发了，赶紧关掉防止 fd 泄露
        return;
    }

    // 1. 打存档 (保存进度)
    fileFd_ = fd;
    fileSize_ = fileSize;
    fileOffset_ = 0;

    // 2. 如果当前没有在监听可写事件，主动唤醒 Epoll
    // (因为 Http Header 刚才可能已经瞬间发完了，我们需要重新让 Epoll 触发
    // handleWrite)
    if (!channel_->isWriting()) {
        channel_->enableWriting();
    }
}

// === 第三阶段：连接断开 ===

// TcpConnection.cpp
void TcpConnection::handleClose() {
    loop_->assertInLoopThread();

    // 🔥 1. 先判定，再打印日志。如果已经断开了，直接悄悄返回。
    if (state_ == kDisconnected) {
        return;
    }

    // day16 新增 判断🔥 保底擦屁股：连接断开时，如果还有文件没发完，赶紧关掉！
    if (fileFd_ >= 0) {
        ::close(fileFd_);
        fileFd_ = -1;
    }

    LOG_INFO << "TcpConnection::handleClose state=" << state_;
    setState(kDisconnected);

    channel_->disableAll();  // 停用所有事件

    TcpConnectionPtr guardThis(shared_from_this());
    if (connectionCallback_) connectionCallback_(guardThis);
    if (closeCallback_)
        closeCallback_(
            guardThis);  // 这一步最终会调用 TcpServer::removeConnection
}
void TcpConnection::handleError() {
    int err = 0;
    socklen_t len = sizeof(err);
    if (::getsockopt(channel_->getFd(), SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
        err = errno;
    }
    LOG_ERROR << "TcpConnection::handleError name:" << name_
              << " - SO_ERROR:" << err;
}

void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::connectDestroyed() {
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnected);
        channel_->disableAll();                   // 停用事件
        connectionCallback_(shared_from_this());  // 回调用户：连接断开了
    }
    channel_->remove();  // 🔥 核心！必须调用它把 FD 彻底踢出 Epoll！
}

// 🔥 Day 19 新增：强制踢人出局
void TcpConnection::forceClose() {
    // 状态机：只要还没彻底断开，就强制打断
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        // 使用 queueInLoop
        // 确保线程安全，并且延长生命周期防止由于过早析构导致的野指针
        loop_->queueInLoop(
            std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

// 🔥 Day 19底层真正执行“拔网线”的地方
void TcpConnection::forceCloseInLoop() {
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting) {
        LOG_INFO << "⚠️ 触发 forceClose: 直接斩断连接 " << name_;
        // 直接复用 handleClose()！
        // handleClose 会调用 channel_->disableAll() 从 Epoll 树上摘除 FD，
        // 并触发 closeCallback_ 让 TcpServer 把这个连接从 Map 里删掉。
        // 最后随着智能指针计数归零，底层的 Socket 析构函数会自动调用
        // ::close(fd)！
        handleClose();
    }
}