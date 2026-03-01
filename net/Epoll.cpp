//
// Created by luna on 2/13/26.
//

#include "Epoll.h"

#include <unistd.h>

#include <cstdio>  // for perror
#include <cstring>

#include "Channel.h"

#define MAX_EVENTS 1000

Epoll::Epoll() : epfd(-1), events(nullptr) {
    epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
    }
    events = new epoll_event[MAX_EVENTS];
}

Epoll::~Epoll() {
    if (epfd != -1) {
        close(epfd);
    }
    delete[] events;
}

// 注册登记
void Epoll::updateChannel(Channel* channel) {
    int fd = channel->getFd();  // 1. 问管家：你负责哪个房间？(获取 fd)
    struct epoll_event ev;  // 2. 准备一张登记表
    bzero(&ev, sizeof(ev));

    // 【最关键的一步！】
    ev.data.ptr = channel;  // 【重要】把 Channel 指针存入 data.ptr  实现了：从
                            // fd (C语言世界) 到 Channel (C++对象世界) 的映射。
    ev.events = channel->getEvents();  // 3. 问管家：你要监控什么？(读? 写?)

    // 4. 根据情况操作内核
    if (!channel->getInEpoll()) {
        // 如果之前没在监控列表里，就“新增” (EPOLL_CTL_ADD)
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
        channel->setInEpoll(true);  // 标记一下：已经在监控了
    } else {
        // 如果已经在列表里了，只是想改需求（比如本来想读，现在想写），就“修改”
        // (EPOLL_CTL_MOD)
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }
}

// 核心等待
void Epoll::poll(std::vector<Channel*>& activeChannels) {
    // 1. 阻塞等待 (Wait)
    int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
        perror("epoll wait");
        return;
    }

    // 2. 遍历发生的事件
    for (int i = 0; i < nfds; ++i) {
        // 【核心魔法的下半场】
        // 从 data.ptr 里把之前存进去的 Channel 指针取出来！
        Channel* ch = (Channel*)events[i].data.ptr;
        // 3. 设置实际发生的事件 (revents)
        // 告诉这个管家：你的房间发生了什么事 (比如 events[i].events 是 EPOLLIN)
        ch->setRevents(events[i].events);
        // 4. 把这个活跃的管家扔进篮子里
        activeChannels.push_back(ch);
    }
}

// day10 【新增】实现 removeChannel
void Epoll::removeChannel(Channel* channel) {
    int fd = channel->getFd();

    // 检查是否真的在 Epoll 中
    if (channel->getInEpoll()) {
        // 从内核事件表中删除
        // EPOLL_CTL_DEL: 删除事件
        // 最后一个参数在 Linux 2.6.9 后可以传 NULL
        if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            perror("epoll_ctl del error");
        }

        // 更新 Channel 状态，表示已经不在 Epoll 监控中了
        channel->setInEpoll(false);
    }
}
