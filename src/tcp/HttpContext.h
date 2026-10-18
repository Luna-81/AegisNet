//
// Created by luna on 2/24/26.
//

#pragma once

#include "HttpRequest.h"

class Buffer;

class HttpContext {
   public:

    enum HttpRequestParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll
    };

    HttpContext() : state_(kExpectRequestLine) {}

    bool parseRequest(Buffer* buf);

    bool gotAll() const { return state_ == kGotAll; }
    void reset() {
        state_ = kExpectRequestLine;
        request_ = HttpRequest();
    }

    const HttpRequest& request() const { return request_; }

   private:

    bool processRequestLine(const char* begin, const char* end);

    HttpRequestParseState state_;
    HttpRequest request_;
};
