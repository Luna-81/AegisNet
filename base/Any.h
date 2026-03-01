//
// Created by luna on 2/24/26.
//

#pragma once

#include <algorithm>
#include <memory>
#include <typeinfo>

class Any {
   public:
    Any() : content_(nullptr) {}

    // 模板构造函数：可以接收任何类型的变量
    template <typename ValueType>
    Any(const ValueType& value) : content_(new Holder<ValueType>(value)) {}

    // 拷贝构造
    Any(const Any& other)
        : content_(other.content_ ? other.content_->clone() : nullptr) {}

    // 移动构造
    Any(Any&& other) noexcept : content_(std::move(other.content_)) {}

    // 赋值运算符
    Any& operator=(const Any& other) {
        Any(other).swap(*this);
        return *this;
    }

    bool empty() const { return !content_; }

    const std::type_info& type() const {
        return content_ ? content_->type() : typeid(void);
    }

    void swap(Any& other) { std::swap(content_, other.content_); }

    // 只有 Any 类内部能看到的基类
    class Placeholder {
       public:
        virtual ~Placeholder() {}
        virtual const std::type_info& type() const = 0;
        virtual Placeholder* clone() const = 0;
    };

    // 存储具体数据的模板类
    template <typename ValueType>
    class Holder : public Placeholder {
       public:
        Holder(const ValueType& value) : held_(value) {}
        const std::type_info& type() const override {
            return typeid(ValueType);
        }
        Placeholder* clone() const override { return new Holder(held_); }
        ValueType held_;
    };

   private:
    // 关键：在这里用基类指针实现类型擦除
    template <typename ValueType>
    friend ValueType* any_cast(Any*);

    std::unique_ptr<Placeholder> content_;
};

// --- 全局转型函数 ---
template <typename ValueType>
ValueType* any_cast(Any* operand) {
    return (operand && operand->type() == typeid(ValueType))
               ? &static_cast<Any::Holder<ValueType>*>(operand->content_.get())
                      ->held_
               : nullptr;
}