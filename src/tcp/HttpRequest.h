//
// Created by luna on 2/24/26.
//

#pragma once

#include <iostream>
#include <string_view>
#include <unordered_map>

class HttpRequest {
   public:
    enum Method { kInvalid, kGet, kPost, kHead, kPut, kDelete };
    enum Version { kUnknown, kHttp10, kHttp11 };

    HttpRequest() : method_(kInvalid), version_(kUnknown) {}

    // These views refer to the input buffer and must not outlive its storage.
    // --- Setter ---
    void setVersion(Version v) { version_ = v; }
    void setMethod(Method m) { method_ = m; }

    void setPath(const char* start, const char* end) {
        path_ = std::string_view(start, end - start);
    }
    void setQuery(const char* start, const char* end) {
        query_ = std::string_view(start, end - start);
    }

    void addHeader(const char* start, const char* colon, const char* end) {
        std::string_view field(start, colon - start);  // Key

        ++colon;
        while (colon < end && isspace(*colon)) ++colon;

        std::string_view value(colon, end - colon);  // Value

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

    std::string_view path_;
    std::string_view query_;

    std::unordered_map<std::string_view, std::string_view> headers_;
};
