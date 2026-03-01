//
// Created by luna on 2/22/26.
//

#include "AsyncLogging.h"

#include <cassert>
#include <cstdio>
#include <functional>

#include "LogFile.h"

AsyncLogging::AsyncLogging(const std::string& basename, int flushInterval)
    : flushInterval_(flushInterval),
      running_(false),
      basename_(basename),
      thread_(std::bind(&AsyncLogging::threadFunc, this), "AsyncLogging"),
      currentBuffer_(new Buffer),
      nextBuffer_(new Buffer) {
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16);
}

void AsyncLogging::start() {
    running_ = true;
    thread_.start();
}

void AsyncLogging::stop() {
    running_ = false;
    cond_.notify_one();
    if (thread_.started()) {
        thread_.join();
    }
}

void AsyncLogging::append(const char* logline, int len) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (currentBuffer_->avail() > len) {
        currentBuffer_->append(logline, len);
    } else {
        // 🔥 探头 1：当 currentBuffer_ 被写满时，打印提示！ 做4MB压力的测试代码
        // printf("\n🔥 [前端探头] 4MB 缓冲区已满！当前积攒了 %d 个满
        // Buffer。唤醒后台！\n", (int)buffers_.size() + 1);
        buffers_.push_back(std::move(currentBuffer_));
        if (nextBuffer_) {
            currentBuffer_ = std::move(nextBuffer_);
        } else {
            currentBuffer_.reset(new Buffer);
        }
        currentBuffer_->append(logline, len);
        cond_.notify_one();
    }
}

void AsyncLogging::threadFunc() {
    // 步骤 1：发车前的准备（开门营业）     准备两个干净的buffer
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->bzero();
    newBuffer2->bzero();

    BufferVector buffersToWrite;  // 操作工具
    buffersToWrite.reserve(16);

    // 🔥 换上我们的新装备！指定基础名，并设定 50MB 滚动 (50 * 1024 * 1024)
    LogFile output(basename_, 50 * 1024 * 1024);

    while (running_) {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);
        assert(buffersToWrite.empty());

        {  //   ⚡ 步骤 2：核心临界区（偷天换日，极速交换）
            std::unique_lock<std::mutex> lock(mutex_);
            if (buffers_.empty()) {
                cond_.wait_for(lock, std::chrono::seconds(flushInterval_));
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
            buffersToWrite.swap(buffers_);
            if (!nextBuffer_) {
                nextBuffer_ = std::move(newBuffer2);
            }
        }
        // 步骤 3：慢吞吞的卸货（无锁写磁盘）
        //  Write without locks
        for (const auto& buffer : buffersToWrite) {
            // ✅ 让管家来写！如果超过 50MB，管家会自动切新文件！
            output.append(buffer->data(), buffer->length());
        }
        output.flush();  // ✅ 让管家去刷新

        if (buffersToWrite.size() > 2) {
            buffersToWrite.resize(2);
        }

        // 🔥 探头 2：当后台醒来并成功换走数据时！做4MB压力的测试代码
        // printf("⚡ [后台探头] 被唤醒！成功换走 %d 个满
        // Buffer，开始无锁写磁盘...\n\n", (int)buffersToWrite.size());

        // ♻️ 步骤 4：打扫战场（环保回收箱子）
        if (!newBuffer1) {
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }
        if (!newBuffer2) {
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }
        buffersToWrite.clear();

        // 🔥 加上这一句！因为刚刚发生过 swap，我们要确保 buffersToWrite
        // 重新变回一个拥有 16 个“空车位”的集装箱。
        // 等下一轮 swap 时，前端的 buffers_ 就能瞬间获得这 16
        // 个车位，彻底告别动态扩容！
        if (buffersToWrite.capacity() < 16) {
            buffersToWrite.reserve(16);
        }
    }
}
