#pragma once

#include <functional>
#include <memory>

class Buffer;
class TcpConnection;
class Timestamp;

// 1. 定义智能指针
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

// 2. 定义回调函数类型
// 注意：这里我们使用 TcpConnectionPtr，而不是原始的 std::shared_ptr
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

// MessageCallback 通常需要 (连接, 缓冲区, 接收时间)
// 如果你现在的 Buffer 还没有 Timestamp，可以先去掉 Timestamp 参数
// 👈 加上 Timestamp 参数
using MessageCallback = std::function<void(
    const std::shared_ptr<TcpConnection>&, Buffer*, Timestamp)>;