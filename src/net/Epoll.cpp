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

void Epoll::updateChannel(Channel* channel) {
    int fd = channel->getFd();
    struct epoll_event ev;
    bzero(&ev, sizeof(ev));

    ev.data.ptr = channel;

    ev.events = channel->getEvents();

    if (!channel->getInEpoll()) {

        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
        channel->setInEpoll(true);
    } else {

        // (EPOLL_CTL_MOD)
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }
}

void Epoll::poll(std::vector<Channel*>& activeChannels) {

    int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
        perror("epoll wait");
        return;
    }

    for (int i = 0; i < nfds; ++i) {

        Channel* ch = (Channel*)events[i].data.ptr;

        ch->setRevents(events[i].events);

        activeChannels.push_back(ch);
    }
}

void Epoll::removeChannel(Channel* channel) {
    int fd = channel->getFd();

    if (channel->getInEpoll()) {

        if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            perror("epoll_ctl del error");
        }

        channel->setInEpoll(false);
    }
}
