#include "../net/Socket.h"

#include <fcntl.h>       // fcntl
#include <sys/socket.h>  // socket, bind, listen...
#include <unistd.h>      // close

#include <iostream>

// 构造函数
Socket::Socket(int fd) : fd_(fd) {
    if (fd_ == -1) {
        std::cerr << "Create socket error!" << "\n";
    }
}

// 默认构造函数 (如果你 main.cpp 里用 new Socket() 无参的话需要这个)
Socket::Socket() : fd_(-1) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        std::cerr << "Create socket error!" << "\n";
    }
}

// 析构函数
Socket::~Socket() {
    if (fd_ != -1) {
        ::close(fd_);
        std::cout << "Socket closed: " << fd_ << "\n";
    }
}

void Socket::bindAddress(const InetAddress& localaddr) {
    // 必须检查返回值！
    if (::bind(fd_, (sockaddr*)localaddr.getAddr(), sizeof(sockaddr_in)) != 0) {
        perror(
            "❌ Socket::bindAddress fatal error");  // 打印错误原因（如 Address
                                                    // already in use）
        exit(1);  // 绑定失败直接退出程序，不要硬撑
    }
    printf("✅ Socket::bindAddress success\n");  // 调试日志
}

void Socket::listen() {
    // 必须检查返回值！
    if (::listen(fd_, SOMAXCONN) != 0) {
        perror("❌ Socket::listen fatal error");
        exit(1);
    }
    printf("✅ Socket::listen success\n");  // 调试日志
}

int Socket::accept(InetAddress* peeraddr) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);

    // 使用 accept4 可以直接设为非阻塞且 CloseOnExec
    // 或者保持 ::accept，但 ListenFd 必须是非阻塞的
    int connfd =
        ::accept4(fd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (connfd < 0) {
        int savedErrno = errno;
        if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK) {
            // 这不是错误，只是连接被别人先抢走了或者被 RST 了
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

// 【关键修改】变成了 Socket 类的成员函数
void Socket::setNonBlocking() {
    int old_option = fcntl(fd_, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd_, F_SETFL, new_option);
}

// 【关键修改】补上了 getFd
int Socket::getFd() { return fd_; }

void Socket::setKeepAlive(bool on) {
    // setsockopt 是 C 语言接口，不认 bool，只认 int (1开启, 0关闭)
    int optval = on ? 1 : 0;
    // ::setsockopt 是系统调用
    // SOL_SOCKET: 这是一个 Socket 层面的选项
    // SO_KEEPALIVE: 选项名叫“保持活跃”
    ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

// day10 【新增实现】
void Socket::shutdownWrite() {
    if (::shutdown(fd_, SHUT_WR) < 0) {
        perror("Socket::shutdownWrite");
    }
}
