#pragma once

#include <sys/time.h>

#include <iostream>
#include <string>

class Timestamp {
   public:

    Timestamp() : microSecondsSinceEpoch_(0) {}

    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

    static Timestamp now() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        int64_t seconds = tv.tv_sec;
        return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
    }

    bool operator<(const Timestamp& rhs) const {
        return this->microSecondsSinceEpoch_ < rhs.microSecondsSinceEpoch_;
    }

    bool operator==(const Timestamp& rhs) const {
        return this->microSecondsSinceEpoch_ == rhs.microSecondsSinceEpoch_;
    }

    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    static const int kMicroSecondsPerSecond = 1000 * 1000;

   private:
    int64_t microSecondsSinceEpoch_;
};

inline Timestamp addTime(Timestamp timestamp, double seconds) {
    int64_t delta =
        static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}