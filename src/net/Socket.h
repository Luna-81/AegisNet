#pragma once
#include "InetAddress.h"

// Owns a socket descriptor and provides the low-level socket operations.
class Socket {
   private:
    int fd_;

   public:
    Socket();
    Socket(int fd);
    ~Socket();

    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept(InetAddress* peeraddr);

    void setReuseAddr(bool on);

    void setNonBlocking();

    int getFd();
    void setKeepAlive(bool on);
    void shutdownWrite();
};
