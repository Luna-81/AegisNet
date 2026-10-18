#include "src/base/Logger.h"
#include "src/net/EventLoop.h"
#include "src/tcp/TcpConnection.h"

#include "test_support.h"

#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <string>
#include <thread>

int main() {
    int sockets[2];
    REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
                         sockets) == 0);

    Logger::setOutput([](const char*, int) {});

    EventLoop loop;
    auto connection = std::make_shared<TcpConnection>(
        &loop, "backpressure-test", sockets[0], InetAddress(), InetAddress());
    connection->setConnectionCallback([](const TcpConnectionPtr&) {});

    std::atomic<bool> highWaterMarkReached{false};
    std::atomic<bool> lowWaterMarkReached{false};
    std::atomic<bool> writeCompleted{false};
    connection->setOutputBufferWatermarks(
        8 * 1024, 32 * 1024,
        [&lowWaterMarkReached](const TcpConnectionPtr&, size_t) {
            lowWaterMarkReached = true;
        },
        [&highWaterMarkReached](const TcpConnectionPtr&, size_t) {
            highWaterMarkReached = true;
        });
    connection->setWriteCompleteCallback(
        [&loop, &writeCompleted](const TcpConnectionPtr&) {
            writeCompleted = true;
            loop.quit();
        });
    connection->connectEstablished();

    const std::string payload(1024 * 1024, 'x');
    connection->send(payload);

    std::string received;
    received.reserve(payload.size());
    std::atomic<bool> consumerFailed{false};
    std::thread consumer([&]() {
        char bytes[8192];
        while (received.size() < payload.size()) {
            const ssize_t n = ::read(sockets[1], bytes, sizeof(bytes));
            if (n > 0) {
                received.append(bytes, static_cast<size_t>(n));
            } else if (n < 0 &&
                       (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                consumerFailed = true;
                loop.quit();
                return;
            }
        }
    });

    loop.loop();
    consumer.join();

    connection->connectDestroyed();
    connection.reset();
    ::close(sockets[1]);

    REQUIRE(highWaterMarkReached.load());
    REQUIRE(lowWaterMarkReached.load());
    REQUIRE(writeCompleted.load());
    REQUIRE(!consumerFailed.load());
    REQUIRE(received == payload);
}
