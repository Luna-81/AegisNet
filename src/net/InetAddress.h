#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstring>
#include <string>

class InetAddress {
   public:
    InetAddress() { std::memset(&addr_, 0, sizeof(addr_)); }

    InetAddress(const char *ip, uint16_t port);

    explicit InetAddress(sockaddr_in addr);

    const sockaddr_in *getAddr() const { return &addr_; }

    void setAddr(sockaddr_in addr) { addr_ = addr; }

    std::string toIpPort() const;

    static InetAddress getLocalAddr(int sockfd);

   private:

    sockaddr_in addr_;
};
