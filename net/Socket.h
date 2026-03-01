#pragma once
#include "InetAddress.h"

class Socket {
   private:
    int fd_;  // 统一使用带下划线的命名风格，区分成员变量

   public:
    Socket();        // 默认构造
    Socket(int fd);  // 带参构造
    ~Socket();       // 析构

    void bindAddress(const InetAddress& localaddr);  // 绑定地址
    void listen();                                   // 开始监听
    int accept(InetAddress* peeraddr);               // 接受连接

    void setReuseAddr(bool on);  // 设置地址复用

    // 【关键】这就是刚才报错缺少的函数声明
    void setNonBlocking();  // 设置非阻塞,删掉了之前写的util.h,直接移入Socket.h

    int getFd();                 // 获取 fd
    void setKeepAlive(bool on);  // day6 新加的
    void shutdownWrite();        // day 10【新增声明】
};