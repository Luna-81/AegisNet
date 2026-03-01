//
// Created by luna on 2/24/26.
//

#include "HttpResponse.h"

#include <string>

#include "Buffer.h"

void HttpResponse::appendToBuffer(Buffer* output) const {
    std::string response;

    // 1. 拼装响应行 (例如: HTTP/1.1 200 OK\r\n)
    response += "HTTP/1.1 " + std::to_string(statusCode_) + " ";
    if (statusCode_ == k200Ok) {
        response += "OK\r\n";
    } else if (statusCode_ == k404NotFound) {
        response += "Not Found\r\n";
    } else if (statusCode_ == k400BadRequest) {
        response += "Bad Request\r\n";
    } else {
        response += "Unknown\r\n";
    }

    // 2. 拼装必要的响应头
    if (closeConnection_) {
        response += "Connection: close\r\n";
    } else {
        response += "Connection: Keep-Alive\r\n";
    }

    // day16 新增🌟 修复点：检查是否已经手动设置了 Content-Length
    // (发文件时我们会手动设)
    if (headers_.find("Content-Length") == headers_.end()) {
        // 如果没有手动设置，且有 body 数据，再用 body_.size()
        response += "Content-Length: " + std::to_string(body_.size()) + "\r\n";
    }
    // 拼装用户自定义的响应头 (包括刚刚手动设的 Content-Length)
    for (const auto& header : headers_) {
        response += header.first + ": " + header.second + "\r\n";
    }

    // 3. 拼装空行 (这是 HTTP 协议的硬性规定，代表头部结束)
    response += "\r\n";

    // 4. 拼装响应体 (也就是你的 HTML 内容)
    response += body_;

    // 5. 一次性将装配好的报文塞进底层 Buffer
    output->append(response.data(), response.size());
}

// day16 🌟 新增实现
std::string HttpResponse::getMimeType(const std::string& filepath) {
    size_t pos = filepath.find_last_of('.');
    if (pos == std::string::npos) return "text/plain";

    std::string ext = filepath.substr(pos);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".ico") return "image/x-icon";

    return "application/octet-stream";  // 默认二进制流
}
