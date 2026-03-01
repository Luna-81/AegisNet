//
// Created by luna on 2/24/26.
//

#pragma once
#include <map>
#include <string>

#include "Buffer.h"

class HttpResponse {
   public:
    enum HttpStatusCode {
        kUnknown,
        k200Ok = 200,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k404NotFound = 404,
    };

    explicit HttpResponse(bool close)
        : statusCode_(kUnknown), closeConnection_(close) {}

    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    void setStatusMessage(const std::string& message) {
        statusMessage_ = message;
    }
    void setCloseConnection(bool on) { closeConnection_ = on; }
    bool closeConnection() const { return closeConnection_; }

    void setContentType(const std::string& contentType) {
        addHeader("Content-Type", contentType);
    }
    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }
    void setBody(const std::string& body) { body_ = body; }

    // 核心：把对象转换成可以发送的 Buffer 字节流
    void appendToBuffer(Buffer* output) const;

    // day16 🌟 新增：作为一个公开的静态工具函数
    static std::string getMimeType(const std::string& filepath);

   private:
    HttpStatusCode statusCode_;
    std::string statusMessage_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    bool closeConnection_;
};