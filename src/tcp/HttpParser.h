//
// Created by luna on 3/1/26.
//

#pragma once
#include <string>
#include <map>
#include "Buffer.h"

// A parser instance retains progress across calls; keep it with its connection.
class HttpParser {
public:
    enum HttpRequestParseState {
        ExpectRequestLine,
        ExpectHeaders,
        ExpectBody,
        GotAll,
        Error,
    };

    HttpParser() : state_(ExpectRequestLine), expectedBodyLength_(0) {}

    bool parseRequest(Buffer* buf);
    void reset();

    bool gotAll() const { return state_ == GotAll; }

    const std::string& getMethod() const { return method_; }
    const std::string& getPath() const { return path_; }
    const std::string& getBody() const { return body_; }
    std::string getHeader(const std::string& field) const;

private:
    bool processRequestLine(const char* begin, const char* end);

    HttpRequestParseState state_;
    std::string method_;
    std::string path_;
    std::string version_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    size_t expectedBodyLength_;
};
