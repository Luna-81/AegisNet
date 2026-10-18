//
// Created by luna on 2/13/26.
//

#pragma once
#include <sys/epoll.h>

#include <vector>

class Channel;

// Polls the kernel epoll instance and maps ready events back to Channels.
class Epoll {
   private:
    int epfd;
    struct epoll_event* events;
   public:
    Epoll();
    ~Epoll();

    void addFd(int fd, uint32_t op);

    void poll(std::vector<Channel*>& activeChannels);
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
};
