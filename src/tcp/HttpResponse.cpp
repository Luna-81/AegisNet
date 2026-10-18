//
// Created by luna on 2/24/26.
//

#include "HttpResponse.h"

#include <string>

#include "Buffer.h"

void HttpResponse::appendToBuffer(Buffer* output) const {
    std::string response;

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

    if (closeConnection_) {
        response += "Connection: close\r\n";
    } else {
        response += "Connection: Keep-Alive\r\n";
    }

    if (headers_.find("Content-Length") == headers_.end()) {

        response += "Content-Length: " + std::to_string(body_.size()) + "\r\n";
    }

    for (const auto& header : headers_) {
        response += header.first + ": " + header.second + "\r\n";
    }

    response += "\r\n";

    response += body_;

    output->append(response.data(), response.size());
}

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

    return "application/octet-stream";
}
