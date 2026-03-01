//
// Created by luna on 2/22/26.
//

#pragma once

#include <cstdio>
#include <string>

#include "noncopyable.h"

// 专职负责写磁盘和日志滚动的管家
class LogFile : noncopyable {
   public:
    // 默认按照 50MB (50 * 1024 * 1024 bytes) 进行滚动切分
    LogFile(const std::string& basename, size_t rollSize = 50 * 1024 * 1024);
    ~LogFile();

    void append(const char* logline, size_t len);
    void flush();

   private:
    void rollFile();  // 触发滚动的核心动作
    static std::string getLogFileName(const std::string& basename);

    const std::string basename_;
    const size_t rollSize_;
    size_t writtenBytes_;  // 记录当前文件已经写了多少字节
    FILE* fp_;

    int lastDay_;  // 新增：记录当前文件是哪一天的 (1~31)
};