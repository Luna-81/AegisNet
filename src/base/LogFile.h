//
// Created by luna on 2/22/26.
//

#pragma once

#include <cstdio>
#include <string>

#include "noncopyable.h"

// Writes log records to rolling files; callers serialize access to this object.
class LogFile : noncopyable {
   public:

    LogFile(const std::string& basename, size_t rollSize = 50 * 1024 * 1024);
    ~LogFile();

    void append(const char* logline, size_t len);
    void flush();

   private:
    bool rollFile();
    static std::string getLogFileName(const std::string& basename);

    const std::string basename_;
    const size_t rollSize_;
    size_t writtenBytes_;
    FILE* fp_;

    int lastYear_;
    int lastDayOfYear_;
};
