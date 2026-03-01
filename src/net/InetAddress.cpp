#include "../net/InetAddress.h"

#include <arpa/inet.h>

#include <cstring>

InetAddress::InetAddress(const char *ip, uint16_t port) {
    // 1. 清零
    std::memset(&addr_, 0, sizeof(addr_));
    // 2. 设置地址族
    addr_.sin_family = AF_INET;
    // 3. 设置端口号 (重点难点！)
    addr_.sin_port = htons(port);

    inet_pton(AF_INET, ip, &addr_.sin_addr);
}

// 【构造函数 2 的实现】
InetAddress::InetAddress(sockaddr_in addr) : addr_(addr) {}

std::string InetAddress::toIpPort() const {
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    uint16_t port = ntohs(addr_.sin_port);
    return std::string(buf) + ":" + std::to_string(port);
}

InetAddress InetAddress::getLocalAddr(int sockfd) {
    struct sockaddr_in localaddr;
    socklen_t addrlen = sizeof(localaddr);
    ::memset(&localaddr, 0, sizeof(localaddr));
    ::getsockname(sockfd, (struct sockaddr *)&localaddr, &addrlen);
    return InetAddress(localaddr);
}
