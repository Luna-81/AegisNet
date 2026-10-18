//
// Created by luna on 2/24/26.
//
#include "HttpContext.h"

#include <algorithm>

#include "Buffer.h"

bool HttpContext::parseRequest(Buffer* buf) {
    bool ok = true;
    bool hasMore = true;

    while (hasMore) {
        if (state_ == kExpectRequestLine) {

            const char* crlf = buf->findCRLF();
            if (crlf) {

                ok = processRequestLine(buf->peek(), crlf);
                if (ok) {
                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders;
                } else {
                    hasMore = false;
                }
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectHeaders) {
            const char* crlf = buf->findCRLF();
            if (crlf) {
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf) {

                    request_.addHeader(buf->peek(), colon, crlf);
                } else {

                    state_ = kGotAll;
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2);
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectBody) {

        }
    }
    return ok;
}

bool HttpContext::processRequestLine(const char* begin, const char* end) {
    bool succeed = false;
    const char* start = begin;
    const char* space = std::find(start, end, ' ');

    if (space != end) {
        std::string method(start, space);
        if (method == "GET")
            request_.setMethod(HttpRequest::kGet);
        else if (method == "POST")
            request_.setMethod(HttpRequest::kPost);

        start = space + 1;
        space = std::find(start, end, ' ');

        if (space != end) {
            request_.setPath(start, space);
            start = space + 1;

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