//
// Created by luna on 2/24/26.
//

#pragma once

#include "HttpRequest.h"

class Buffer;

class HttpContext {
   public:
    // 解析状态：请求行 -> 请求头 -> 请求体 -> 完成
    enum HttpRequestParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll
    };

    HttpContext() : state_(kExpectRequestLine) {}

    // 核心解析入口：处理来自 Buffer 的数据
    bool parseRequest(Buffer* buf);

    bool gotAll() const { return state_ == kGotAll; }
    void reset() {
        state_ = kExpectRequestLine;
        request_ = HttpRequest();  // 重置 Request 对象
    }

    const HttpRequest& request() const { return request_; }

   private:
    // 解析请求行 (GET /index.html HTTP/1.1)
    bool processRequestLine(const char* begin, const char* end);

    HttpRequestParseState state_;
    HttpRequest request_;
};
