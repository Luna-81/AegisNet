//
// Created by luna on 2/24/26.
//
#include "HttpContext.h"

#include <algorithm>

#include "Buffer.h"

// 处理来自 Buffer 的数据流
bool HttpContext::parseRequest(Buffer* buf) {
    bool ok = true;
    bool hasMore = true;

    while (hasMore) {
        if (state_ == kExpectRequestLine) {
            // 1. 寻找行尾 \r\n
            const char* crlf = buf->findCRLF();
            if (crlf) {
                // 找到了一行，开始解析请求行
                ok = processRequestLine(buf->peek(), crlf);
                if (ok) {
                    buf->retrieveUntil(crlf + 2);  // 从缓冲区移除已处理的行
                    state_ = kExpectHeaders;  // 下一步：解析请求头
                } else {
                    hasMore = false;
                }
            } else {
                hasMore = false;  // 数据不够一行，等下一次数据包
            }
        } else if (state_ == kExpectHeaders) {
            const char* crlf = buf->findCRLF();
            if (crlf) {
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf) {
                    // 解析到一个键值对 Header
                    request_.addHeader(buf->peek(), colon, crlf);
                } else {
                    // 没找到冒号，说明遇到了空行，Header 结束了！
                    state_ = kGotAll;
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2);  // 跨过 \r\n
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectBody) {
            // TODO: 解析 Body（今天先实现 GET，暂不处理 Body）
        }
    }
    return ok;
}

// 解析请求行：GET /index.html HTTP/1.1
bool HttpContext::processRequestLine(const char* begin, const char* end) {
    bool succeed = false;
    const char* start = begin;
    const char* space = std::find(start, end, ' ');

    // 1. 解析 Method
    if (space != end) {
        std::string method(start, space);
        if (method == "GET")
            request_.setMethod(HttpRequest::kGet);
        else if (method == "POST")
            request_.setMethod(HttpRequest::kPost);
        // ... 其他 Method

        start = space + 1;
        space = std::find(start, end, ' ');

        // 2. 解析 URL
        if (space != end) {
            request_.setPath(start, space);
            start = space + 1;

            // 3. 解析 Version
            std::string version(start, end);
            if (version == "HTTP/1.0") {
                request_.setVersion(HttpRequest::kHttp10);
                succeed = true;
            } else if (version == "HTTP/1.1") {
                request_.setVersion(HttpRequest::kHttp11);
                succeed = true;
            }
        }
    }
    return succeed;
}