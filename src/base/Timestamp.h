#pragma once

#include <sys/time.h>

#include <iostream>
#include <string>

class Timestamp {
   public:
    // 默认构造，代表无效时间
    Timestamp() : microSecondsSinceEpoch_(0) {}

    // 显式构造
    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

    // 核心 1：获取当前时间 (静态方法)
    static Timestamp now() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        int64_t seconds = tv.tv_sec;
        return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
    }

    // 核心 2：重载 < 运算符，为了能在 std::set 里按时间从小到大排序
    bool operator<(const Timestamp& rhs) const {
        return this->microSecondsSinceEpoch_ < rhs.microSecondsSinceEpoch_;
    }

    bool operator==(const Timestamp& rhs) const {
        return this->microSecondsSinceEpoch_ == rhs.microSecondsSinceEpoch_;
    }

    // 返回底层的微秒数
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    // 1 秒 = 1000000 微秒
    static const int kMicroSecondsPerSecond = 1000 * 1000;

   private:
    int64_t microSecondsSinceEpoch_;
};

// 核心 3：时间加法工具函数，比如计算 10 秒后的时间戳
inline Timestamp addTime(Timestamp timestamp, double seconds) {
    int64_t delta =
        static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}