//
// Created by luna on 3/1/26.
//

#pragma once
#include <string>
#include <map>
#include "Buffer.h" // 引用你之前写的 Buffer 类

class HttpParser {
public:
    enum HttpRequestParseState {
        ExpectRequestLine,
        ExpectHeaders,
        ExpectBody,
        GotAll,
    };

    HttpParser() : state_(ExpectRequestLine) {}

    // 解析入口
    bool parseRequest(Buffer* buf);

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
};