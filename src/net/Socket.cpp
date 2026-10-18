#include "../net/Socket.h"

#include <fcntl.h>       // fcntl
#include <sys/socket.h>  // socket, bind, listen...
#include <unistd.h>      // close

#include <iostream>

Socket::Socket(int fd) : fd_(fd) {
    if (fd_ == -1) {
        std::cerr << "Create socket error!" << "\n";
    }
}

Socket::Socket() : fd_(-1) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        std::cerr << "Create socket error!" << "\n";
    }
}

Socket::~Socket() {
    if (fd_ != -1) {
        ::close(fd_);
        std::cout << "Socket closed: " << fd_ << "\n";
    }
}

void Socket::bindAddress(const InetAddress& localaddr) {

    if (::bind(fd_, (sockaddr*)localaddr.getAddr(), sizeof(sockaddr_in)) != 0) {
        perror(
            "❌ Socket::bindAddress fatal error");
                                                    // already in use）
        exit(1);
    }
    printf("✅ Socket::bindAddress success\n");
}

void Socket::listen() {

    if (::listen(fd_, SOMAXCONN) != 0) {
        perror("❌ Socket::listen fatal error");
        exit(1);
    }
    printf("✅ Socket::listen success\n");
}

int Socket::accept(InetAddress* peeraddr) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);

    int connfd =
        ::accept4(fd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (connfd < 0) {
        int savedErrno = errno;
        if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK) {

            return -1;
        }
    }
    if (connfd >= 0) {
        peeraddr->setAddr(addr);
    }
    return connfd;
}

void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setNonBlocking() {
    int old_option = fcntl(fd_, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd_, F_SETFL, new_option);
}

int Socket::getFd() { return fd_; }

void Socket::setKeepAlive(bool on) {

    int optval = on ? 1 : 0;

    ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

void Socket::shutdownWrite() {
    if (::shutdown(fd_, SHUT_WR) < 0) {
        perror("Socket::shutdownWrite");
    }
}
