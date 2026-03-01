#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>  // for size_t
#include <string>
#include <vector>

class Buffer {
   public:
    static constexpr size_t kCheapPrepend =
        8;  // day11 新增：预留 8 字节头部空间
    static constexpr size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize);
    ~Buffer() = default;

    // --- 状态查询 (保留在头文件以便内联优化) ---
    size_t readableBytes() const { return writeIndex_ - readIndex_; }
    size_t writableBytes() const { return buffer_.size() - writeIndex_; }
    size_t prependableBytes() const { return readIndex_; }

    // --- 数据预览 ---
    const char* peek() const { return begin() + readIndex_; }

    // --- 数据操作 (声明) ---
    void retrieve(size_t len);
    void retrieveAll();

    std::string retrieveAllAsString();
    std::string retrieveAsString(size_t len);

    // --- 协议解析辅助 (Day 15 新增) ---
    static const char kCRLF[];  // 定义 HTTP 的换行符 \r\n

    // 在当前可读区域寻找 \r\n
    const char* findCRLF() const {
        // 在 [peek(), beginWrite()) 范围内搜索 "\r\n"
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
        return crlf == beginWrite() ? nullptr : crlf;
    }

    // 重回指定位置（通常是 crlf 之后），回收已读数据
    void retrieveUntil(const char* end) {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }
    //----------------------------------

    void append(const std::string& str);
    void append(const char* data, size_t len);

    // --- 核心 IO 接口 ---
    ssize_t readFd(int fd, int* savedErrno);

    // --- 辅助接口 ---
    char* beginWrite() { return begin() + writeIndex_; }
    const char* beginWrite() const { return begin() + writeIndex_; }

    // --- 协议解析辅助 ---
    // 偷看头部的 4 字节（不移动读指针）
    int32_t peekInt32() const;
    // 读取并消耗 4 字节
    int32_t readInt32();

    // 往预留空间（头部）写入数据
    void prepend(const void* data, size_t len);
    void prependInt32(int32_t x);

   private:
    // 内部私有函数
    char* begin() { return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }

    // 扩容逻辑声明
    void makeSpace(size_t len);

    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;
};