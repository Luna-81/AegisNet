#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>

#include <cstring>
#include <string>

class InetAddress {
   public:
    InetAddress() { std::memset(&addr_, 0, sizeof(addr_)); }  // 默认构造函数
    // 【构造函数 1】
    // 用途：给你 IP 字符串（如 "192.168.1.1"）和端口号（如
    // 8888），创建一个地址对象。
    InetAddress(const char *ip, uint16_t port);

    // 【构造函数 2】
    // 用途：如果你手里已经有了一个系统原本的 sockaddr_in
    // 结构体，直接用它来创建对象。
    explicit InetAddress(sockaddr_in addr);

    // 【获取内部原始数据】
    // 用途：把内部藏着的那个系统结构体拿出来给别人看（比如给 bind 函数用）。
    const sockaddr_in *getAddr() const { return &addr_; }

    // 补上这一行
    void setAddr(sockaddr_in addr) { addr_ = addr; }
    // 【转成字符串】
    std::string toIpPort() const;
    // day 7 添加
    static InetAddress getLocalAddr(int sockfd);

   private:
    // 【核心数据】
    // 这就是那个最重要的、系统内核认识的“身份证”。
    sockaddr_in addr_;
};
