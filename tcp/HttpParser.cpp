//
// Created by luna on 3/1/26.
//
#include "HttpParser.h"
#include <algorithm>

bool HttpParser::parseRequest(Buffer* buf) {
    bool ok = true;
    bool hasMore = true;
    while (hasMore) {
        if (state_ == ExpectRequestLine) {
            const char* crlf = buf->findCRLF();
            if (crlf) {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok) {
                    buf->retrieveUntil(crlf + 2);
                    state_ = ExpectHeaders;
                } else {
                    hasMore = false;
                }
            } else {
                hasMore = false;
            }
        } else if (state_ == ExpectHeaders) {
            const char* crlf = buf->findCRLF();
            if (crlf) {
                const char* colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf) {
                    headers_[std::string(buf->peek(), colon)] = std::string(colon + 2, crlf);
                } else {
                    // 空行，表示 Header 结束
                    if (headers_.count("Content-Length")) {
                        state_ = ExpectBody;
                    } else {
                        state_ = GotAll;
                        hasMore = false;
                    }
                }
                buf->retrieveUntil(crlf + 2);
            } else {
                hasMore = false;
            }
        } else if (state_ == ExpectBody) {
            size_t len = std::stoi(headers_["Content-Length"]);
            if (buf->readableBytes() >= len) {
                body_.assign(buf->peek(), len);
                buf->retrieve(len);
                state_ = GotAll;
            }
            hasMore = false;
        }
    }
    return ok;
}

bool HttpParser::processRequestLine(const char* begin, const char* end) {
    std::string line(begin, end);
    size_t space1 = line.find(' ');
    size_t space2 = line.find(' ', space1 + 1);
    if (space1 != std::string::npos && space2 != std::string::npos) {
        method_ = line.substr(0, space1);
        path_ = line.substr(space1 + 1, space2 - space1 - 1);
        version_ = line.substr(space2 + 1);
        return true;
    }
    return false;
}

std::string HttpParser::getHeader(const std::string& field) const {
    auto it = headers_.find(field);
    if (it != headers_.end()) return it->second;
    return "";
}