#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
   public:
    // 定义回调函数的类型
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    // 构造函数
    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);

    ~EventLoopThreadPool();

    // 设置线程数
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    // 启动线程池
    // 注意：默认参数 = ThreadInitCallback() 只能写在这里！
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 获取下一个 Loop (轮询)
    EventLoop *getNextLoop();

    // 获取所有 Loop
    std::vector<EventLoop *> getAllLoops();

    bool started() const { return started_; }
    const std::string &name() const { return name_; }

   private:
    // 1、管理层 (指挥与状态)
    EventLoop
        *baseLoop_;  // 站长 / 总调度. 作用：这是主线程的 Loop（MainReactor）
    bool started_;  // 营业状态牌,  作用：防止重复启动
    int next_;      // 派单指针,  实现 Round-Robin (轮询) 调度

    // 2、配置层 (参数设置)
    std::string name_;  // 站点名称, 调试和日志用
    int numThreads_;    // 招聘名额 ,作用：决定要开几个子线程

    // 3、资源层 (核心资产)
    std::vector<std::unique_ptr<EventLoopThread> >
        threads_;  // 骑手本人 (生命周期管理) 作用：管“生”管“死”
    std::vector<EventLoop *> loops_;  // 骑手的手机 (工作接口)  作用：管“干活”
};
