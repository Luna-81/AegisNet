#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

class Thread {
   public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string &name = std::string());

    ~Thread();

    void start();

    void join();

    bool started() const { return started_; }
    const std::string &name() const { return name_; }

   private:
    // 1. 📇 身份系统 (Identity)
    static std::atomic_int
        numCreated_;  // 全局工号发号机,作用：每创建一个新特工，它就自动 +1。
    std::string name_;  // 角色：特工代号。 作用：方便你调试
    void
    setDefaultName();  // 角色：起名逻辑。
                       // 作用：如果你创建特工时没给他起名，这个函数就会被调用。

    // 2. 🚦 状态系统 (State Control)
    bool started_;  // 角色：任务状态灯。   作用：记录“特工出发了吗？”
    bool joined_;  // 角色：回归状态灯。   作用：记录“特工回来汇报工作了吗？”
                   // join() 的意思是主线程等待子线程结束并回收资源。

    // 3. ⚙️ 执行系统 (Execution Engine)
    std::shared_ptr<std::thread>
        thread_;  // 真正的肉身  这是 C++11 标准库的 std::thread 对象
    ThreadFunc
        func_;  // 角色：任务密函
                // 作用：特工具体要干什么？（比如：去监听端口、去计算数据）。
};

// 变量名,                 作用,             对应特工的比喻
// numCreated_,         全局计数,            发号机 (累计发了多少工牌)
// name_,               线程名,              工牌 (Thread-1)
// started_,            是否已启动,           任务状态 (出勤中)
// joined_,             是否已汇合,           打卡状态 (已下班汇报)
// func_,               线程回调函数,         任务书 (具体要干的活)
// thread_,             线程对象指针,         特工本人 (只有 start 时才招募进来)
