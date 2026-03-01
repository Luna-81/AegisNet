//
// Created by luna on 2/22/26.
//

#pragma once

#include <cstring>
#include <string>
#include <vector>
#include <string_view>

#include "noncopyable.h"

namespace detail {
const int kSmallBuffer = 4000;  // 4KB  用于 LogStream
const int kLargeBuffer =
    4000 *
    1000;  // 4MB  用于 AsyncLogging 后端  这是大批量的缓冲，减少写磁盘的次数。
}  // namespace detail

template <int SIZE>
class FixedBuffer : noncopyable {
   public:
    FixedBuffer() : cur_(data_) {}
    ~FixedBuffer() = default;

    // 核心搬运工。先检查剩下的空间够不够，够的话直接用 memcpy
    // 把数据烤进来，然后挪动 cur_ 指针。
    void append(const char* buf, size_t len) {
        if (static_cast<size_t>(avail()) > len) {
            std::memcpy(cur_, buf, len);
            cur_ += len;
        }
    }

    const char* data() const {
        return data_;
    }  // 返回数组的首地址（用于读数据）。
    int length() const {
        return static_cast<int>(cur_ - data_);
    }  // 计算当前已经存了多少字节（cur_ - data_）。
    char* current() { return cur_; }  // 返回 cur_ 指针，告诉外部现在写到哪了。
    int avail() const {
        return static_cast<int>(end() - cur_);
    }  // 计算还剩多少空间可用（end() - cur_）。
    void add(size_t len) {
        cur_ += len;
    }  // 手动挪动 cur_。有时外部直接往 current() 里写数据，写完后调用 add 告诉
       // FixedBuffer 进度。
    void reset() {
        cur_ = data_;
    }  // 把 cur_ 重新指向 data_ 的开头，逻辑上清空了数据，但内存没变。
    void bzero() {
        std::memset(data_, 0, sizeof(data_));
    }  // 物理清空。用 memset 把整块内存抹成 0。

   private:
    const char* end() const {
        return data_ + sizeof(data_);
    }  // 返回数组末尾的下一个地址，作为边界判定。
    char data_
        [SIZE];  // 真正的物理存储空间，
                 // 避免了动态内存分配（malloc/new），这是性能高的根本原因。
    char* cur_;  // 写指针。它指向当前数组中第一个“空位”。
};

class LogStream : noncopyable {
   public:
    using Buffer = FixedBuffer<detail::kSmallBuffer>;

    // operator<<
    // (各种重载)：这实现了链式调用，每一个重载都负责把对应类型转成字符串
    LogStream& operator<<(bool v) {
        buffer_.append(v ? "1" : "0", 1);
        return *this;
    }
    LogStream& operator<<(const char* str) {
        if (str)
            buffer_.append(str, strlen(str));
        else
            buffer_.append("(null)", 6);
        return *this;
    }
    LogStream& operator<<(const std::string& v) {
        buffer_.append(v.c_str(), v.size());
        return *this;
    }

    template <typename T>  // 作用：把整数（int, long等）转换成字符。
    LogStream& formatInteger(T v) {
        std::string str =
            std::to_string(v);  // For simplicity. Industrial uses custom itoa.
        buffer_.append(str.c_str(), str.size());
        return *this;
    }

    LogStream& operator<<(short v) { return formatInteger(v); }
    LogStream& operator<<(unsigned short v) { return formatInteger(v); }
    LogStream& operator<<(int v) { return formatInteger(v); }
    LogStream& operator<<(unsigned int v) { return formatInteger(v); }
    LogStream& operator<<(long v) { return formatInteger(v); }
    LogStream& operator<<(long long v) { return formatInteger(v); }
    LogStream& operator<<(double v) {
        std::string str = std::to_string(v);
        buffer_.append(str.c_str(), str.size());
        return *this;
    }
    //  新增：专门处理 unsigned long (在 64 位下通常就是 size_t)
    LogStream& operator<<(unsigned long v) { return formatInteger(v); }
    LogStream& operator<<(char v) {
        buffer_.append(&v, 1);
        return *this;
    }
    // day15🔥  新增std::string_view 的输出
    LogStream& operator<<(std::string_view v) {
        buffer_.append(v.data(), v.size());
        return *this;
    }

    const Buffer& buffer() const {
        return buffer_;
    }  // 获取内部那个 FixedBuffer 的引用，通常在 Logger
       // 析构时调用，把攒好的数据拿走。
    void resetBuffer() {
        buffer_.reset();
    }  // 清空内部缓冲区，准备接下一条日志。

   private:
    Buffer
        buffer_;  // 内部持有一个 kSmallBuffer (4KB) 规格的 FixedBuffer。所有的
                  // << 操作最终都写到了这里。
};