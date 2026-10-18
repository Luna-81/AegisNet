#include "src/base/Logger.h"
#include "src/net/EventLoop.h"
#include "src/tcp/TcpConnection.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <memory>
#include <string>

int main() {
    int sockets[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
                     sockets) < 0) {
        std::perror("socketpair");
        return 1;
    }

    Logger::setOutput([](const char*, int) {});

    EventLoop loop;
    auto connection = std::make_shared<TcpConnection>(
        &loop, "socketpair-test", sockets[0], InetAddress(), InetAddress());
    connection->setConnectionCallback([](const TcpConnectionPtr&) {});

    bool receivedRequest = false;
    connection->setMessageCallback(
        [&loop, &receivedRequest](const TcpConnectionPtr& conn, Buffer* buffer,
                                  Timestamp) {
            const std::string request = buffer->retrieveAllAsString();
            receivedRequest = request == "ping";
            if (receivedRequest) {
                conn->send("pong");
            }
            loop.quit();
        });
    connection->connectEstablished();

    const bool requestSent = ::write(sockets[1], "ping", 4) == 4;
    if (requestSent) {
        loop.loop();
    }

    char response[4] = {};
    const bool responseReceived = ::read(sockets[1], response, sizeof(response)) == 4;
    const bool responseMatches = responseReceived &&
                                 std::string(response, sizeof(response)) == "pong";

    connection->connectDestroyed();
    connection.reset();
    ::close(sockets[1]);

    if (!requestSent || !receivedRequest || !responseMatches) {
        std::fprintf(stderr, "TCP connection round trip failed\n");
        return 1;
    }
}
