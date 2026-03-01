//
// Created by luna on 2/24/26.
//

#pragma once

#include <iostream>
#include <string_view>    // 🔥 引入 C++17 的零拷贝神器
#include <unordered_map>  // 换成哈希表，查找比 map（红黑树）更快

class HttpRequest {
   public:
    enum Method { kInvalid, kGet, kPost, kHead, kPut, kDelete };
    enum Version { kUnknown, kHttp10, kHttp11 };

    HttpRequest() : method_(kInvalid), version_(kUnknown) {}

    // --- Setter ---
    void setVersion(Version v) { version_ = v; }
    void setMethod(Method m) { method_ = m; }

    // 🔥 零拷贝：直接记录原始 Buffer 中的指针范围
    void setPath(const char* start, const char* end) {
        path_ = std::string_view(start, end - start);
    }
    void setQuery(const char* start, const char* end) {
        query_ = std::string_view(start, end - start);
    }

    // 🔥 零拷贝：解析 Header 键值对
    void addHeader(const char* start, const char* colon, const char* end) {
        std::string_view field(start, colon - start);  // Key

        ++colon;
        while (colon < end && isspace(*colon)) ++colon;  // 跳过冒号后的空格

        std::string_view value(colon, end - colon);  // Value

        // 存入哈希表，没有发生任何字符串拷贝！
        headers_[field] = value;
    }

    // --- Getter ---
    std::string_view path() const { return path_; }
    std::string_view query() const { return query_; }
    Method method() const { return method_; }

    std::string_view getHeader(std::string_view field) const {
        auto it = headers_.find(field);
        return (it != headers_.end()) ? it->second : std::string_view();
    }

   private:
    Method method_;
    Version version_;

    // 🔥 全部换成 string_view
    std::string_view path_;
    std::string_view query_;

    // 🔥 使用 unordered_map 提高查找性能，并且 Key 和 Value 全是指针引用
    std::unordered_map<std::string_view, std::string_view> headers_;
};