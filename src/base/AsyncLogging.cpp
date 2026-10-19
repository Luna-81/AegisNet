#include "AsyncLogging.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <utility>

#include "LogFile.h"

AsyncLogging::AsyncLogging(const std::string& basename, int flushInterval)
    : currentBuffer_(new Buffer),
      nextBuffer_(new Buffer),
      running_(false),
      thread_(std::bind(&AsyncLogging::threadFunc, this), "AsyncLogging"),
      basename_(basename),
      flushInterval_(std::max(1, flushInterval)) {
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16);
}

void AsyncLogging::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) return;

    started_ = true;
    running_ = true;
    thread_.start();
}

void AsyncLogging::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_) return;
        running_ = false;
    }
    cond_.notify_one();
    std::call_once(joinOnce_, [this]() {
        if (thread_.started()) thread_.join();
    });
}

void AsyncLogging::append(const char* logline, int len) {
    if (logline == nullptr || len <= 0) return;

    std::unique_lock<std::mutex> lock(mutex_);
    if (!running_) {
        lock.unlock();
        std::fwrite(logline, 1, static_cast<size_t>(len), stderr);
        std::fflush(stderr);
        return;
    }

    bool queuedBuffer = false;
    size_t offset = 0;
    const size_t totalLength = static_cast<size_t>(len);
    while (offset < totalLength) {
        if (currentBuffer_->avail() == 0) {
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = nextBuffer_ ? std::move(nextBuffer_)
                                         : BufferPtr(new Buffer);
            queuedBuffer = true;
        }

        const size_t chunkLength = std::min(
            static_cast<size_t>(currentBuffer_->avail()), totalLength - offset);
        if (!currentBuffer_->append(logline + offset, chunkLength)) break;
        offset += chunkLength;

        if (currentBuffer_->avail() == 0) {
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = nextBuffer_ ? std::move(nextBuffer_)
                                         : BufferPtr(new Buffer);
            queuedBuffer = true;
        }
    }

    if (queuedBuffer) cond_.notify_one();
}

void AsyncLogging::threadFunc() {
    BufferPtr spareBuffer1(new Buffer);
    BufferPtr spareBuffer2(new Buffer);
    spareBuffer1->reset();
    spareBuffer2->reset();

    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    LogFile output(basename_, 50 * 1024 * 1024);

    for (;;) {
        bool shouldExit = false;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (buffers_.empty() && currentBuffer_->length() == 0 && running_) {
                cond_.wait_for(lock, std::chrono::seconds(flushInterval_));
            }

            if (currentBuffer_->length() != 0) {
                buffers_.push_back(std::move(currentBuffer_));
            }
            currentBuffer_ = spareBuffer1 ? std::move(spareBuffer1)
                                          : BufferPtr(new Buffer);

            buffersToWrite.swap(buffers_);
            if (!nextBuffer_ && spareBuffer2) {
                nextBuffer_ = std::move(spareBuffer2);
            }
            shouldExit = buffersToWrite.empty() && !running_;
        }

        if (shouldExit) break;

        for (const auto& buffer : buffersToWrite) {
            output.append(buffer->data(), static_cast<size_t>(buffer->length()));
        }
        if (!buffersToWrite.empty()) output.flush();

        for (auto& buffer : buffersToWrite) {
            buffer->reset();
            if (!spareBuffer1) {
                spareBuffer1 = std::move(buffer);
            } else if (!spareBuffer2) {
                spareBuffer2 = std::move(buffer);
            }
        }
        buffersToWrite.clear();
    }
}
