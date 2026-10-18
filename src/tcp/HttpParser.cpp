#include "HttpParser.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <system_error>

namespace {

bool isTokenCharacter(unsigned char ch) {
    if (ch > 0x7f) return false;
    if (std::isalnum(ch)) return true;
    switch (ch) {
        case '!':
        case '#':
        case '$':
        case '%':
        case '&':
        case '\'':
        case '*':
        case '+':
        case '-':
        case '.':
        case '^':
        case '_':
        case '`':
        case '|':
        case '~':
            return true;
        default:
            return false;
    }
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool parseContentLength(const std::string& value, size_t* length) {
    if (value.empty()) return false;
    for (unsigned char ch : value) {
        if (!std::isdigit(ch)) return false;
    }

    size_t parsed = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end) return false;

    *length = parsed;
    return true;
}

}  // namespace

void HttpParser::reset() {
    state_ = ExpectRequestLine;
    method_.clear();
    path_.clear();
    version_.clear();
    headers_.clear();
    body_.clear();
    expectedBodyLength_ = 0;
}

bool HttpParser::parseRequest(Buffer* buf) {
    if (buf == nullptr || state_ == Error) return false;
    if (state_ == GotAll) return true;

    while (true) {
        if (state_ == ExpectRequestLine) {
            const char* crlf = buf->findCRLF();
            if (crlf == nullptr) return true;

            if (!processRequestLine(buf->peek(), crlf)) {
                state_ = Error;
                return false;
            }

            buf->retrieveUntil(crlf + 2);
            state_ = ExpectHeaders;
            continue;
        }

        if (state_ == ExpectHeaders) {
            const char* crlf = buf->findCRLF();
            if (crlf == nullptr) return true;

            if (crlf == buf->peek()) {
                buf->retrieveUntil(crlf + 2);

                // Reject unsupported framing to avoid ambiguous request boundaries.
                if (headers_.count("transfer-encoding") != 0) {
                    state_ = Error;
                    return false;
                }

                const auto contentLength = headers_.find("content-length");
                if (contentLength == headers_.end()) {
                    state_ = GotAll;
                    return true;
                }

                if (!parseContentLength(contentLength->second, &expectedBodyLength_)) {
                    state_ = Error;
                    return false;
                }
                state_ = ExpectBody;
                continue;
            }

            const char* lineBegin = buf->peek();
            const char* colon = std::find(lineBegin, crlf, ':');
            if (colon == lineBegin || colon == crlf) {
                state_ = Error;
                return false;
            }

            std::string field(lineBegin, colon);
            for (unsigned char ch : field) {
                if (!isTokenCharacter(ch)) {
                    state_ = Error;
                    return false;
                }
            }
            field = toLowerAscii(std::move(field));

            const char* valueBegin = colon + 1;
            while (valueBegin < crlf && (*valueBegin == ' ' || *valueBegin == '\t')) {
                ++valueBegin;
            }
            const char* valueEnd = crlf;
            while (valueEnd > valueBegin &&
                   (valueEnd[-1] == ' ' || valueEnd[-1] == '\t')) {
                --valueEnd;
            }
            for (const char* p = valueBegin; p < valueEnd; ++p) {
                const unsigned char ch = static_cast<unsigned char>(*p);
                if ((ch < 0x20 && ch != '\t') || ch == 0x7f) {
                    state_ = Error;
                    return false;
                }
            }

            std::string value(valueBegin, valueEnd);
            if (!headers_.emplace(field, std::move(value)).second) {
                state_ = Error;
                return false;
            }

            buf->retrieveUntil(crlf + 2);
            continue;
        }

        if (state_ == ExpectBody) {
            if (buf->readableBytes() < expectedBodyLength_) return true;

            body_.assign(buf->peek(), expectedBodyLength_);
            // Leave any following pipelined request in the buffer for the next parse.
            buf->retrieve(expectedBodyLength_);
            state_ = GotAll;
            return true;
        }
    }
}

bool HttpParser::processRequestLine(const char* begin, const char* end) {
    const char* firstSpace = std::find(begin, end, ' ');
    if (firstSpace == begin || firstSpace == end) return false;

    const char* secondSpace = std::find(firstSpace + 1, end, ' ');
    if (secondSpace == firstSpace + 1 || secondSpace == end) return false;
    if (std::find(secondSpace + 1, end, ' ') != end) return false;

    for (const char* p = begin; p < firstSpace; ++p) {
        if (!isTokenCharacter(static_cast<unsigned char>(*p))) return false;
    }
    for (const char* p = firstSpace + 1; p < secondSpace; ++p) {
        const unsigned char ch = static_cast<unsigned char>(*p);
        if (ch <= 0x20 || ch == 0x7f) return false;
    }

    const std::string version(secondSpace + 1, end);
    if (version != "HTTP/1.0" && version != "HTTP/1.1") return false;

    method_.assign(begin, firstSpace);
    path_.assign(firstSpace + 1, secondSpace);
    version_ = version;
    return true;
}

std::string HttpParser::getHeader(const std::string& field) const {
    const auto it = headers_.find(toLowerAscii(field));
    if (it != headers_.end()) return it->second;
    return "";
}
