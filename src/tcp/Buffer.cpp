#include "Buffer.h"

#include <arpa/inet.h>
#include <errno.h>    // for errno
#include <sys/uio.h>  // for readv, iovec
#include <unistd.h>   // for ssize_t

#include <cassert>
#include <cstring>

const char Buffer::kCRLF[] = "\r\n";

Buffer::Buffer(size_t initialSize)
    : buffer_(kCheapPrepend + initialSize),
      readIndex_(kCheapPrepend),
      writeIndex_(kCheapPrepend) {}

void Buffer::retrieve(size_t len) {
    if (len < readableBytes()) {
        readIndex_ += len;
    } else {
        retrieveAll();
    }
}

void Buffer::retrieveAll() {
    readIndex_ = kCheapPrepend;
    writeIndex_ = kCheapPrepend;
}

std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

std::string Buffer::retrieveAsString(size_t len) {
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

void Buffer::append(const std::string &str) { append(str.data(), str.size()); }

void Buffer::append(const char *data, size_t len) {
    if (writableBytes() < len) {
        makeSpace(len);
    }

    std::copy(data, data + len, beginWrite());
    writeIndex_ += len;
}

void Buffer::makeSpace(size_t len) {

    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        buffer_.resize(writeIndex_ + len);
    } else {

        size_t readable = readableBytes();
        std::copy(begin() + readIndex_, begin() + writeIndex_,
                  begin() + kCheapPrepend);
        readIndex_ = kCheapPrepend;
        writeIndex_ = readIndex_ + readable;
    }
}

ssize_t Buffer::readFd(int fd, int *savedErrno) {

    char extrabuf[65536];

    struct iovec vec[2];
    const size_t writable = writableBytes();

    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {

        writeIndex_ += n;
    } else {

        writeIndex_ = buffer_.size();
        append(extrabuf,
               n - writable);
    }

    return n;
}

int32_t Buffer::peekInt32() const {

    assert(readableBytes() >= sizeof(int32_t));

    int32_t be32 = 0;

    ::memcpy(&be32, peek(), sizeof(be32));

    return ::ntohl(be32);
}

int32_t Buffer::readInt32() {
    int32_t result = peekInt32();
    retrieve(sizeof(int32_t));
    return result;
}

void Buffer::prepend(const void *data, size_t len) {

    assert(len <= prependableBytes());
    readIndex_ -= len;
    const char *d = static_cast<const char *>(data);
    std::copy(d, d + len, begin() + readIndex_);
}

void Buffer::prependInt32(int32_t x) {
    int32_t be32 = ::htonl(x);
    prepend(&be32, sizeof(be32));
}