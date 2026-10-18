#include "src/tcp/HttpParser.h"

#include "test_support.h"

int main() {
    // These baseline cases use complete requests; fragmented input is tested with parser fixes.
    {
        Buffer buffer;
        buffer.append("GET /health HTTP/1.1\r\nHost: localhost\r\nX-Trace: request-1\r\n\r\n");

        HttpParser parser;
        REQUIRE(parser.parseRequest(&buffer));
        REQUIRE(parser.gotAll());
        REQUIRE(parser.getMethod() == "GET");
        REQUIRE(parser.getPath() == "/health");
        REQUIRE(parser.getHeader("Host") == "localhost");
        REQUIRE(buffer.readableBytes() == 0);
    }

    {
        Buffer buffer;
        buffer.append("POST /chat HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello");

        HttpParser parser;
        REQUIRE(parser.parseRequest(&buffer));
        REQUIRE(parser.gotAll());
        REQUIRE(parser.getMethod() == "POST");
        REQUIRE(parser.getPath() == "/chat");
        REQUIRE(parser.getBody() == "hello");
        REQUIRE(buffer.readableBytes() == 0);
    }
}
