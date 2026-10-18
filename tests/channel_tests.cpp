#include "src/net/Channel.h"
#include "src/net/EventLoop.h"

#include "test_support.h"

#include <sys/socket.h>
#include <unistd.h>

int main() {
    int sockets[2];
    REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0,
                         sockets) == 0);

    EventLoop loop;
    Channel channel(&loop, sockets[0]);
    channel.enableReading();
    channel.enableWriting();

    REQUIRE(channel.isReading());
    REQUIRE(channel.isWriting());
    REQUIRE((channel.getEvents() & EPOLLET) == 0);

    channel.remove();
    ::close(sockets[0]);
    ::close(sockets[1]);
}
