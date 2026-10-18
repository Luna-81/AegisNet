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
const int kSmallBuffer = 4000;
const int kLargeBuffer =
    4000 *
    1000;
}  // namespace detail

template <int SIZE>
class FixedBuffer : noncopyable {
   public:
    FixedBuffer() : cur_(data_) {}
    ~FixedBuffer() = default;

    void append(const char* buf, size_t len) {
        if (static_cast<size_t>(avail()) > len) {
            std::memcpy(cur_, buf, len);
            cur_ += len;
        }
    }

    const char* data() const {
        return data_;
    }
    int length() const {
        return static_cast<int>(cur_ - data_);
    }
    char* current() { return cur_; }
    int avail() const {
        return static_cast<int>(end() - cur_);
    }
    void add(size_t len) {
        cur_ += len;
    }

    void reset() {
        cur_ = data_;
    }
    void bzero() {
        std::memset(data_, 0, sizeof(data_));
    }

   private:
    const char* end() const {
        return data_ + sizeof(data_);
    }
    char data_
        [SIZE];

    char* cur_;
};

class LogStream : noncopyable {
   public:
    using Buffer = FixedBuffer<detail::kSmallBuffer>;

    // operator<<

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

    template <typename T>
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

    LogStream& operator<<(unsigned long v) { return formatInteger(v); }
    LogStream& operator<<(char v) {
        buffer_.append(&v, 1);
        return *this;
    }

    LogStream& operator<<(std::string_view v) {
        buffer_.append(v.data(), v.size());
        return *this;
    }

    const Buffer& buffer() const {
        return buffer_;
    }

    void resetBuffer() {
        buffer_.reset();
    }

   private:
    Buffer
        buffer_;

};