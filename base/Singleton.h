//
// Created by luna on 2/26/26.
//

#pragma once

// 这是一个极其经典的 C++11 单例模板
template <typename T>
class Singleton {
   public:
    // 1. 提供全局唯一的访问点 (Meyers' Singleton 的核心)
    static T& getInstance() {
        // 🔥 C++11 魔法：局部静态变量的初始化天生是线程安全的！
        // 编译器会加锁保证 T instance 只会被构造一次
        static T instance;
        return instance;
    }

    // 2. 禁用拷贝和赋值 (禁止克隆)
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

   protected:
    // 3. 构造函数和析构函数 protected，允许子类继承，但外部无法直接 new
    Singleton() = default;
    ~Singleton() = default;
};