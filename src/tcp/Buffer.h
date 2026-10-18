#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>  // for size_t
#include <string>
#include <vector>

// A growable byte buffer with separate read and write positions.
class Buffer {
   public:
    static constexpr size_t kCheapPrepend =
        8;
    static constexpr size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize);
    ~Buffer() = default;

    size_t readableBytes() const { return writeIndex_ - readIndex_; }
    size_t writableBytes() const { return buffer_.size() - writeIndex_; }
    size_t prependableBytes() const { return readIndex_; }

    const char* peek() const { return begin() + readIndex_; }

    void retrieve(size_t len);
    void retrieveAll();

    std::string retrieveAllAsString();
    std::string retrieveAsString(size_t len);

    static const char kCRLF[];

    const char* findCRLF() const {

        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
        return crlf == beginWrite() ? nullptr : crlf;
    }

    void retrieveUntil(const char* end) {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }
    //----------------------------------

    void append(const std::string& str);
    void append(const char* data, size_t len);

    ssize_t readFd(int fd, int* savedErrno);

    char* beginWrite() { return begin() + writeIndex_; }
    const char* beginWrite() const { return begin() + writeIndex_; }

    int32_t peekInt32() const;

    int32_t readInt32();

    void prepend(const void* data, size_t len);
    void prependInt32(int32_t x);

   private:

    char* begin() { return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len);

    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;
};
