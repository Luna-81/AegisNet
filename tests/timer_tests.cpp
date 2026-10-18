#include "src/net/EventLoop.h"

#include "test_support.h"

#include <atomic>
#include <chrono>
#include <thread>

int main() {
    EventLoop loop;
    REQUIRE(loop.runEvery(0.0, []() {}) == 0);
    std::atomic<int> repeatCount{0};
    std::atomic<int> canceledCount{0};
    std::atomic<bool> crossThreadTimerFired{false};
    std::atomic<bool> earlyCheckPassed{false};

    const auto startedAt = std::chrono::steady_clock::now();
    const TimerId repeatingTimer =
        loop.runEvery(0.05, [&]() { ++repeatCount; });
    const TimerId canceledTimer = loop.runEvery(
        0.02, [&]() { ++canceledCount; });

    loop.runAfter(0.02, [&]() {
        earlyCheckPassed = repeatCount.load() == 0;
    });
    loop.runAfter(0.125, [&]() { loop.cancel(repeatingTimer); });
    loop.runAfter(0.18, [&]() { loop.quit(); });

    std::thread submitter([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        loop.cancel(canceledTimer);
        loop.runAfter(0.01, [&]() { crossThreadTimerFired = true; });
    });

    loop.loop();
    submitter.join();

    const auto elapsed = std::chrono::steady_clock::now() - startedAt;
    REQUIRE(earlyCheckPassed.load());
    REQUIRE(crossThreadTimerFired.load());
    REQUIRE(canceledCount.load() == 0);
    REQUIRE(repeatCount.load() == 2);
    REQUIRE(elapsed >= std::chrono::milliseconds(170));
}
