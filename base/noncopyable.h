#pragma once

/**
 * noncopyable 被继承后，派生类对象就会自动禁止拷贝构造和赋值操作
 */
class noncopyable {
   public:
    // 拷贝构造函数 (Copy Constructor)
    //= delete  这个函数删了，谁也不许用
    noncopyable(const noncopyable&) = delete;
    // 赋值运算符 (Assignment Operator)， 同上
    void operator=(const noncopyable&) = delete;

   protected:
    // 默认构造函数：按默认的方式来就行
    noncopyable() = default;
    ~noncopyable() = default;
};