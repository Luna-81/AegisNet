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

/**
 * TcpConnection
 * -------------------------
 * 封装了 socket handle，管理连接的生命周期。
 * 它是唯一一个由 shared_ptr
 * 管理的类，因为它的生命周期模糊（用户、Server、Channel 都持有它）。
 */
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
   public:
    // 构造函数：由 TcpServer 使用
    TcpConnection(EventLoop* loop, const std::string& name, int sockfd,
                  const InetAddress& localAddr, const InetAddress& peerAddr);
    ~TcpConnection();

    // === 1. 基础信息查询 ===
    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }

    int getFd() const;

    // === 2. 核心发送与关闭 ===
    // 线程安全，可以跨线程调用
    void send(const std::string& buf);
    void shutdown();  // 优雅关闭（只关闭写端）

    // 🔥 Day 19 新增：强制关闭连接（防 DdoS 僵尸连接）
    void forceClose();

    // === 3. 设置回调函数 ===
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

    // === 4. 给 TcpServer 调用的生命周期钩子 ===
    void connectEstablished();  // 连接建立成功时调用
    void connectDestroyed();    // 连接彻底销毁前调用

    // day15 设置上下文 (存入 std::any)
    void setContext(const std::any& context) { context_ = context; }

    // day15 获取上下文的指针 (用于修改状态机)
    std::any* getMutableContext() { return &context_; }

    // 🔥 Day 16 核心：发起零拷贝发送文件的请求
    void sendFile(int fd, size_t fileSize);

   private:
    // 内部状态枚举
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

    void setState(StateE s) { state_ = s; }

    // === 5. Channel 的回调函数 (私有) ===
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();  // 处理对端关闭 (read return 0)
    void handleError();

    // === 6. 跨线程执行的具体逻辑 ===
    // 对应 send()，但在 IO 线程执行
    void sendInLoop(const std::string& message);
    // 对应 shutdown()，但在 IO 线程执行
    void shutdownInLoop();

    void forceCloseInLoop();  // day19 新增🔥

    // day16 新增🔥 供 EventLoop 线程内部调用的发文件逻辑
    void sendFileInLoop(int fd, size_t fileSize);

    // day16 新增🔥 发送大文件时的状态机存档变量
    int fileFd_ = -1;  // 正在发送的文件句柄 (-1 表示当前没在发文件)
    size_t fileSize_ = 0;   // 文件总大小
    off_t fileOffset_ = 0;  // 已经发送了多少字节 (必须用 off_t)

    // === 7. 成员变量 ===

    // 核心组件
    EventLoop* loop_;            // 所属 EventLoop
    const std::string name_;     // 连接名称
    std::atomic<StateE> state_;  // 原子状态
    bool reading_;  // 是否正在监听读事件 (可选，用于高级控制)

    // 底层封装
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    // 地址信息
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 回调函数保存
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;  // 低水位回调（流量控制）
    CloseCallback closeCallback_;  // 内部回调：通知 TcpServer 移除自己

    // 数据缓冲区
    Buffer inputBuffer_;   // 接收缓冲区
    Buffer outputBuffer_;  // 发送缓冲区

    std::any context_;  // day15🔥 这里的 context_ 换成标准库的万能口袋
};