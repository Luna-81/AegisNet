#include "LogFile.h"

#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

LogFile::LogFile(const std::string& basename, size_t rollSize)
    : basename_(basename),
      rollSize_(rollSize),
      writtenBytes_(0),
      fp_(nullptr),
      lastYear_(-1),
      lastDayOfYear_(-1) {
    rollFile();
}

LogFile::~LogFile() {
    if (fp_) {
        if (::fflush(fp_) != 0) {
            std::fprintf(stderr, "LogFile: fflush failed: %s\n",
                         std::strerror(errno));
        }
        if (::fclose(fp_) != 0) {
            std::fprintf(stderr, "LogFile: fclose failed: %s\n",
                         std::strerror(errno));
        }
    }
}

void LogFile::append(const char* logline, size_t len) {
    if (len == 0) return;
    if (logline == nullptr) {
        std::fprintf(stderr, "LogFile: refusing to write a null log buffer\n");
        return;
    }

    const time_t now = ::time(nullptr);
    struct tm localTime {};
    if (::localtime_r(&now, &localTime) == nullptr) {
        std::fprintf(stderr, "LogFile: localtime_r failed\n");
        return;
    }

    const bool dayChanged = localTime.tm_year != lastYear_ ||
                            localTime.tm_yday != lastDayOfYear_;
    const bool sizeExceeded =
        writtenBytes_ > 0 &&
        (writtenBytes_ >= rollSize_ || len > rollSize_ - writtenBytes_);
    if ((dayChanged || sizeExceeded) && !rollFile()) return;
    if (!fp_) return;

    const size_t written = ::fwrite(logline, 1, len, fp_);
    writtenBytes_ += written;
    if (written != len) {
        std::fprintf(stderr, "LogFile: fwrite wrote %zu of %zu bytes: %s\n",
                     written, len, std::strerror(errno));
    }
}

void LogFile::flush() {
    if (fp_ && ::fflush(fp_) != 0) {
        std::fprintf(stderr, "LogFile: fflush failed: %s\n",
                     std::strerror(errno));
    }
}

bool LogFile::rollFile() {
    FILE* newFile = nullptr;
    std::string newFilename;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        newFilename = getLogFileName(basename_);
        const int fd = ::open(newFilename.c_str(),
                              O_WRONLY | O_APPEND | O_CREAT | O_EXCL | O_CLOEXEC,
                              0644);
        if (fd < 0) {
            if (errno == EEXIST) continue;
            std::fprintf(stderr, "LogFile: cannot create %s: %s\n",
                         newFilename.c_str(), std::strerror(errno));
            return false;
        }

        newFile = ::fdopen(fd, "a");
        if (!newFile) {
            const int savedErrno = errno;
            ::close(fd);
            ::unlink(newFilename.c_str());
            std::fprintf(stderr, "LogFile: fdopen failed for %s: %s\n",
                         newFilename.c_str(), std::strerror(savedErrno));
            return false;
        }
        break;
    }

    if (!newFile) {
        std::fprintf(stderr, "LogFile: unable to find a unique rolling filename\n");
        return false;
    }

    const time_t now = ::time(nullptr);
    struct tm localTime {};
    if (::localtime_r(&now, &localTime) == nullptr) {
        std::fclose(newFile);
        ::unlink(newFilename.c_str());
        std::fprintf(stderr, "LogFile: localtime_r failed while rolling\n");
        return false;
    }

    if (fp_) {
        if (::fflush(fp_) != 0) {
            std::fprintf(stderr, "LogFile: fflush failed while rolling: %s\n",
                         std::strerror(errno));
        }
        if (::fclose(fp_) != 0) {
            std::fprintf(stderr, "LogFile: fclose failed while rolling: %s\n",
                         std::strerror(errno));
        }
    }
    fp_ = newFile;
    writtenBytes_ = 0;
    lastYear_ = localTime.tm_year;
    lastDayOfYear_ = localTime.tm_yday;
    return true;
}

std::string LogFile::getLogFileName(const std::string& basename) {
    static std::atomic<uint64_t> sequence{0};
    struct timeval now {};
    ::gettimeofday(&now, nullptr);

    struct tm localTime {};
    ::localtime_r(&now.tv_sec, &localTime);
    char timebuf[32];
    ::strftime(timebuf, sizeof(timebuf), ".%Y%m%d-%H%M%S", &localTime);

    char uniquebuf[96];
    std::snprintf(uniquebuf, sizeof(uniquebuf), ".%06ld.%ld.%llu.log",
                  static_cast<long>(now.tv_usec), static_cast<long>(::getpid()),
                  static_cast<unsigned long long>(sequence.fetch_add(
                      1, std::memory_order_relaxed)));
    return basename + timebuf + uniquebuf;
}
