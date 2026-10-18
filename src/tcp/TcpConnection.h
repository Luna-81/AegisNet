#pragma once

#include <any>
#include <atomic>
#include <memory>
#include <string>

#include "../base/noncopyable.h"
#include "../net/InetAddress.h"
#include "Buffer.h"
#include "Callbacks.h"

class Channel;
class EventLoop;
class Socket;

// Owns one TCP connection; its Channel and I/O buffers belong to loop_.
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
   public:

    TcpConnection(EventLoop* loop, const std::string& name, int sockfd,
                  const InetAddress& localAddr, const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }

    int getFd() const;

    void send(const std::string& buf);
    void shutdown();

    void forceClose();

    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }

    void connectEstablished();
    void connectDestroyed();

    void setContext(const std::any& context) { context_ = context; }

    std::any* getMutableContext() { return &context_; }

    void sendFile(int fd, size_t fileSize);

   private:

    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

    void setState(StateE s) { state_ = s; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const std::string& message);

    void shutdownInLoop();

    void forceCloseInLoop();

    void sendFileInLoop(int fd, size_t fileSize);

    int fileFd_ = -1;
    size_t fileSize_ = 0;
    off_t fileOffset_ = 0;

    EventLoop* loop_;
    const std::string name_;
    std::atomic<StateE> state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;

    std::any context_;
};
