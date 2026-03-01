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
    rollFile();  // 对象一创建，就立刻打开第一个日志文件
}

LogFile::~LogFile() {
    if (fp_) {
        ::fclose(fp_);
    }
}

// 切文件：大小10MB/1GB 和 1天/1min时间
void LogFile::append(const char *logline, size_t len) {
    if (!fp_) return;

    // 🔥 核心魔法：判断是否跨天了！
    time_t now = time(nullptr);
    struct tm tm_time;
    localtime_r(&now, &tm_time);  // 获取当前本地时间
    // TODO 按天切文件，测试按分钟切文件
    // 正式用按天切文件
    int currentDay = tm_time.tm_mday;  // 取出今天是几号
    if (currentDay != lastDay_) {
        rollFile();  // 强制切分新文件！
    }
    // 如果没跨天，再看文件大小有没有超过阈值 (比如你的 10MB)
    else if (writtenBytes_ > rollSize_) {
        rollFile();  // 强制切分新文件！
    }

    // 以下是测试用按分钟切
    //  🔥 测试专用：把 tm_mday (天) 临时改成 tm_min (分钟)
    //  int currentMinute = tm_time.tm_min;
    //
    //  // 🔥 测试专用：如果当前分钟变了，我们就假装是“跨天（零点）”了！
    //  if (currentMinute != lastDay_) {
    //      rollFile();
    //  }
    //  else if (writtenBytes_ > rollSize_) {
    //      rollFile();
    //  }

    if (!fp_) return;  // 防御性检查
    size_t written = ::fwrite(logline, 1, len, fp_);
    writtenBytes_ += written;
}

void LogFile::flush() {
    if (fp_) {
        ::fflush(fp_);
    }
}

// 滚动切分动作
void LogFile::rollFile() {
    if (fp_) {
        ::fclose(fp_);  // 优雅关闭旧文件
    }
    std::string filename = getLogFileName(basename_);
    fp_ = ::fopen(filename.c_str(), "ae");  // a=追加, e=O_CLOEXEC
    writtenBytes_ = 0;  // 新文件，写入字节数清零！

    // TODO 正式按天切；测试用分钟切
    //  下面是按天切
    time_t now = time(nullptr);
    struct tm tm_time;
    localtime_r(&now, &tm_time);
    lastDay_ = tm_time.tm_mday;
    // lastDay_= tm_time.tm_min;   // 测试用时1分钟
}

// 生成带时间戳的文件名：basename.年月日-时分秒.log
std::string LogFile::getLogFileName(const std::string &basename) {
    time_t now = time(nullptr);
    char timebuf[32];
    struct tm tm_time;
    localtime_r(&now, &tm_time);  // 线程安全的获取本地时间
    strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm_time);

    return basename + timebuf + "log";
}
