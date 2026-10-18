#pragma once

#include <cstddef>
#include <functional>
#include <memory>

class Buffer;
class TcpConnection;
class Timestamp;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using WaterMarkCallback =
    std::function<void(const TcpConnectionPtr&, size_t queuedBytes)>;

using MessageCallback = std::function<void(
    const std::shared_ptr<TcpConnection>&, Buffer*, Timestamp)>;
