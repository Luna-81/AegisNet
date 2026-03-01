#include "Buffer.h"

#include <arpa/inet.h>  // 必须包含这个来使用 ntohl (网络转主机字节序)
#include <errno.h>    // for errno
#include <sys/uio.h>  // for readv, iovec
#include <unistd.h>   // for ssize_t

#include <cassert>
#include <cstring>  // for memcpy (如果有用到)

// 在文件顶部定义换行符常量
const char Buffer::kCRLF[] = "\r\n";

// day 11 改进 1. 构造函数：初始化时，让读写指针从 kCheapPrepend 开始，而不是 0
Buffer::Buffer(size_t initialSize)
    : buffer_(kCheapPrepend + initialSize),
      readIndex_(kCheapPrepend),
      writeIndex_(kCheapPrepend) {}

// 移动读指针
void Buffer::retrieve(size_t len) {
    if (len < readableBytes()) {
        readIndex_ += len;
    } else {
        retrieveAll();
    }
}

// 重置缓冲区
void Buffer::retrieveAll() {
    readIndex_ = kCheapPrepend;
    writeIndex_ = kCheapPrepend;
}

// 读取所有数据转为 string
std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

// 读取指定长度数据转为 string
std::string Buffer::retrieveAsString(size_t len) {
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

// 追加 string
void Buffer::append(const std::string &str) { append(str.data(), str.size()); }

// 追加 char* 数据 (核心写逻辑)
void Buffer::append(const char *data, size_t len) {
    if (writableBytes() < len) {
        makeSpace(len);  // 空间不够，扩容或整理
    }
    // 拷贝数据到可写位置
    std::copy(data, data + len, beginWrite());
    writeIndex_ += len;
}

// 内存管理：扩容或移动数据
// day11 改进 2. makeSpace：整理内存时，不要挪到 0，而是挪到 kCheapPrepend
void Buffer::makeSpace(size_t len) {
    // 如果 [剩余空闲] + [头部废弃空间] 都不够装，只能扩容
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        buffer_.resize(writeIndex_ + len);
    } else {
        // 空间够，只是由于 readIndex 后移导致前面有空隙
        // 把数据挪到最前面 (内部碎片整理)
        size_t readable = readableBytes();
        std::copy(begin() + readIndex_, begin() + writeIndex_,
                  begin() + kCheapPrepend);   // 挪到预留空间的后面
        readIndex_ = kCheapPrepend;           // 移 8 个位置
        writeIndex_ = readIndex_ + readable;  // 移 8 个位置
    }
}

// 从 fd 读取数据 (核心读逻辑)
ssize_t Buffer::readFd(int fd, int *savedErrno) {
    // 栈上临时空间，防止 Buffer 初始太小导致读不完
    char extrabuf[65536];

    struct iovec vec[2];
    const size_t writable = writableBytes();

    // 第一块：Buffer 内部剩余空间
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;

    // 第二块：栈上临时空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // 如果 Buffer 够大，就不需要第二块
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    // 分散读
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        // 第一块就装得下
        writeIndex_ += n;
    } else {
        // 装不下，溢出部分在 extrabuf
        writeIndex_ = buffer_.size();  // Buffer 写满
        append(extrabuf,
               n - writable);  // 把 extrabuf 里的数据追加进 Buffer (会自动扩容)
    }

    return n;
}

int32_t Buffer::peekInt32() const {
    // 确保有足够的数据可读，否则是逻辑错误（调用前业务层应该检查）  应该是 >=
    // ，确保有足够的数据可读
    // 调用前必须由调用者（或上一层协议解析代码）确保有足够的字节
    assert(readableBytes() >= sizeof(int32_t));

    int32_t be32 = 0;
    // 修正 1：从 peek() 开始读，而不是 beginWrite()
    ::memcpy(&be32, peek(), sizeof(be32));

    // 修正 2：网络字节序 -> 主机字节序 使用 ntohl
    return ::ntohl(be32);
}

int32_t Buffer::readInt32() {
    int32_t result = peekInt32();
    retrieve(sizeof(int32_t));  // 偷看完了，指针往后走4字节
    return result;
}

// 往头部预留空间写入数据
void Buffer::prepend(const void *data, size_t len) {
    // 断言：要写入的数据长度不能超过我们预留的头部空间
    // 在真实生产环境中可以使用 assert(len <= prependableBytes());
    assert(len <= prependableBytes());  // 强力拦截越界写入
    readIndex_ -= len;
    const char *d = static_cast<const char *>(data);
    std::copy(d, d + len, begin() + readIndex_);
}

// 专门往头部塞一个 4 字节的整数（包头）
void Buffer::prependInt32(int32_t x) {
    int32_t be32 = ::htonl(x);  // 主机字节序 -> 网络字节序
    prepend(&be32, sizeof(be32));
}