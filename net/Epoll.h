//
// Created by luna on 2/13/26.
//

#pragma once
#include <sys/epoll.h>

#include <vector>

class Channel;  // 前置声明

class Epoll {
   private:
    int epfd;  // 保安室的 ID (操作系统内核给的句柄)
    struct epoll_event* events;  // 保安的记事本 (数组)
   public:
    Epoll();
    ~Epoll();

    void addFd(int fd, uint32_t op);
    // 核心修改：接收一个 vector 引用，直接往里面填数据
    void poll(std::vector<Channel*>& activeChannels);
    void updateChannel(Channel* channel);  // 新增：供 EventLoop 调用
    void removeChannel(Channel* channel);  // day10 新增
};
