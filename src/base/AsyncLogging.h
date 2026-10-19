//
// Created by luna on 2/22/26.
//

#pragma once

#include "LogStream.h"
#include "Thread.h"
// #include "noncopyable.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Singleton.h"

// Buffers log records from producer threads and drains them on stop.
class AsyncLogging : public Singleton<AsyncLogging> {

    friend class Singleton<AsyncLogging>;

   public:
    explicit AsyncLogging(const std::string& basename = "myserver_log",
                          int flushInterval = 3);
    ~AsyncLogging() {
        stop();
    }

    void append(
        const char* logline,
        int len);

    void start();
    // Stops accepting buffered records, drains all accepted data, then joins.
    void
    stop();

   private:
    void threadFunc();

    using Buffer = FixedBuffer<
        detail::
            kLargeBuffer>;

    using BufferPtr = std::unique_ptr<Buffer>;
    using BufferVector = std::vector<BufferPtr>;

    BufferPtr
        currentBuffer_;

    BufferPtr
        nextBuffer_;

    BufferVector
        buffers_;

    std::atomic<bool>
        running_;
    bool started_ = false;
    Thread thread_;
    std::once_flag joinOnce_;

    std::mutex mutex_;
    std::condition_variable
        cond_;

    std::string basename_;

    const int flushInterval_;
};
