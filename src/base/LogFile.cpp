//
// Created by luna on 2/22/26.
//

#include "LogFile.h"

#include <ctime>

LogFile::LogFile(const std::string &basename, size_t rollSize)
    : basename_(basename),
      rollSize_(rollSize),
      writtenBytes_(0),
      fp_(nullptr),
      lastDay_() {
    rollFile();
}

LogFile::~LogFile() {
    if (fp_) {
        ::fclose(fp_);
    }
}

void LogFile::append(const char *logline, size_t len) {
    if (!fp_) return;

    time_t now = time(nullptr);
    struct tm tm_time;
    localtime_r(&now, &tm_time);

    int currentDay = tm_time.tm_mday;
    if (currentDay != lastDay_) {
        rollFile();
    }

    else if (writtenBytes_ > rollSize_) {
        rollFile();
    }

    //  int currentMinute = tm_time.tm_min;
    //

    //  if (currentMinute != lastDay_) {
    //      rollFile();
    //  }
    //  else if (writtenBytes_ > rollSize_) {
    //      rollFile();
    //  }

    if (!fp_) return;
    size_t written = ::fwrite(logline, 1, len, fp_);
    writtenBytes_ += written;
}

void LogFile::flush() {
    if (fp_) {
        ::fflush(fp_);
    }
}

void LogFile::rollFile() {
    if (fp_) {
        ::fclose(fp_);
    }
    std::string filename = getLogFileName(basename_);
    fp_ = ::fopen(filename.c_str(), "ae");
    writtenBytes_ = 0;

    time_t now = time(nullptr);
    struct tm tm_time;
    localtime_r(&now, &tm_time);
    lastDay_ = tm_time.tm_mday;

}

std::string LogFile::getLogFileName(const std::string &basename) {
    time_t now = time(nullptr);
    char timebuf[32];
    struct tm tm_time;
    localtime_r(&now, &tm_time);
    strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm_time);

    return basename + timebuf + "log";
}
