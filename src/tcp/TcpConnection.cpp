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

    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO << "TcpConnection::ctor[" << name_ << "] at " << this
             << " fd=" << sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG_INFO << "TcpConnection::dtor[" << name_ << "] at " << this
             << " fd=" << channel_->getFd() << " state=" << state_;
}

int TcpConnection::getFd() const { return channel_->getFd(); }

void TcpConnection::setOutputBufferWatermarks(
    size_t lowWaterMark, size_t highWaterMark,
    const WaterMarkCallback& lowCallback,
    const WaterMarkCallback& highCallback) {
    assert(lowWaterMark < highWaterMark);
    lowWaterMark_ = lowWaterMark;
    highWaterMark_ = highWaterMark;
    lowWaterMarkCallback_ = lowCallback;
    highWaterMarkCallback_ = highCallback;
}

void TcpConnection::connectEstablished() {
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);

    channel_->tie(shared_from_this());
    channel_->enableReading();

    if (connectionCallback_) {
        connectionCallback_(shared_from_this());
    }
}

void TcpConnection::handleRead(Timestamp receiveTime) {
    loop_->assertInLoopThread();
    int savedErrno = 0;

    ssize_t n = inputBuffer_.readFd(channel_->getFd(), &savedErrno);

    if (likely(n > 0)) {

        if (messageCallback_) {
            messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        }
    } else if (n == 0) {

        handleClose();
    } else {

        errno = savedErrno;
        LOG_ERROR << "TcpConnection::handleRead error";
        handleError();
    }
}

void TcpConnection::handleWrite() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        LOG_INFO << "Connection fd=" << channel_->getFd()
                 << " is down, no more writing";
        return;
    }

    // ==========================================

    // ==========================================
    if (outputBuffer_.readableBytes() > 0) {
        ssize_t n = ::write(channel_->getFd(), outputBuffer_.peek(),
                            outputBuffer_.readableBytes());
        if (n > 0) {
            outputBuffer_.retrieve(n);
        } else if (n < 0 && errno != EAGAIN) {
            LOG_ERROR << "TcpConnection::handleWrite write outputBuffer error";
            return;
        }
    }

    const size_t queuedBytes = outputBuffer_.readableBytes();
    if (aboveHighWaterMark_ && queuedBytes <= lowWaterMark_) {
        aboveHighWaterMark_ = false;
        if (lowWaterMarkCallback_) {
            loop_->queueInLoop(std::bind(lowWaterMarkCallback_, shared_from_this(),
                                         queuedBytes));
        }
    }

    // ==========================================

    // ==========================================
    if (outputBuffer_.readableBytes() == 0 && fileFd_ >= 0) {

        ssize_t n = ::sendfile(channel_->getFd(), fileFd_, &fileOffset_,
                               fileSize_ - fileOffset_);

        if (n > 0) {

        } else if (n < 0) {
            if (errno == EAGAIN) {

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

    // ==========================================

    if (outputBuffer_.readableBytes() == 0 &&
        (fileFd_ < 0 || fileOffset_ == fileSize_)) {

        channel_->disableWriting();

        if (fileFd_ >= 0) {
            ::close(fileFd_);
            fileFd_ = -1;
            LOG_INFO << "✅ 静态文件发送完毕，零拷贝大成功！";
        }

        if (writeCompleteCallback_) {
            loop_->queueInLoop(
                std::bind(writeCompleteCallback_, shared_from_this()));
        }

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

    if (!faultError && remaining > 0) {
        outputBuffer_.append(message.data() + nwrote, remaining);
        const size_t queuedBytes = outputBuffer_.readableBytes();
        if (!aboveHighWaterMark_ && queuedBytes >= highWaterMark_) {
            aboveHighWaterMark_ = true;
            if (highWaterMarkCallback_) {
                loop_->queueInLoop(std::bind(highWaterMarkCallback_,
                                             shared_from_this(), queuedBytes));
            }
        }
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

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

void TcpConnection::sendFileInLoop(int fd, size_t fileSize) {
    loop_->assertInLoopThread();
    if (state_ == kDisconnected) {
        LOG_ERROR << "disconnected, give up sending file";
        ::close(fd);
        return;
    }

    fileFd_ = fd;
    fileSize_ = fileSize;
    fileOffset_ = 0;

    // handleWrite)
    if (!channel_->isWriting()) {
        channel_->enableWriting();
    }
}

// TcpConnection.cpp
void TcpConnection::handleClose() {
    loop_->assertInLoopThread();

    if (state_ == kDisconnected) {
        return;
    }

    if (fileFd_ >= 0) {
        ::close(fileFd_);
        fileFd_ = -1;
    }

    LOG_INFO << "TcpConnection::handleClose state=" << state_;
    setState(kDisconnected);

    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());
    if (connectionCallback_) connectionCallback_(guardThis);
    if (closeCallback_)
        closeCallback_(
            guardThis);
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
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::forceClose() {

    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);

        loop_->queueInLoop(
            std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

void TcpConnection::forceCloseInLoop() {
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting) {
        LOG_INFO << "⚠️ 触发 forceClose: 直接斩断连接 " << name_;

        // ::close(fd)！
        handleClose();
    }
}
