#include "src/tcp/HttpParser.h"

#include "test_support.h"

int main() {
    // Complete requests establish the baseline request and body behavior.
    {
        Buffer buffer;
        buffer.append("GET /health HTTP/1.1\r\nHost: localhost\r\nX-Trace: request-1\r\n\r\n");

        HttpParser parser;
        REQUIRE(parser.parseRequest(&buffer));
        REQUIRE(parser.gotAll());
        REQUIRE(parser.getMethod() == "GET");
        REQUIRE(parser.getPath() == "/health");
        REQUIRE(parser.getHeader("Host") == "localhost");
        REQUIRE(parser.getHeader("host") == "localhost");
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

    {
        Buffer buffer;
        HttpParser parser;

        buffer.append("GET /split HTTP/1.1\r");
        REQUIRE(parser.parseRequest(&buffer));
        REQUIRE(!parser.gotAll());

        buffer.append("\nHost: local");
        REQUIRE(parser.parseRequest(&buffer));
        REQUIRE(!parser.gotAll());

        buffer.append("host\r\n\r\n");
        REQUIRE(parser.parseRequest(&buffer));
        REQUIRE(parser.gotAll());
        REQUIRE(parser.getPath() == "/split");
        REQUIRE(parser.getHeader("Host") == "localhost");
        REQUIRE(buffer.readableBytes() == 0);
    }

    {
        Buffer buffer;
        HttpParser parser;
        buffer.append("POST /partial-body HTTP/1.1\r\nContent-Length: 5\r\n\r\nhe");

        REQUIRE(parser.parseRequest(&buffer));
        REQUIRE(!parser.gotAll());
        REQUIRE(std::string(buffer.peek(), buffer.readableBytes()) == "he");

        buffer.append("llo");
        REQUIRE(parser.parseRequest(&buffer));
        REQUIRE(parser.gotAll());
        REQUIRE(parser.getBody() == "hello");
    }

    {
        Buffer buffer;
        HttpParser first;
        buffer.append("GET /one HTTP/1.1\r\nHost: local\r\n\r\n"
                      "GET /two HTTP/1.1\r\nHost: local\r\n\r\n");

        REQUIRE(first.parseRequest(&buffer));
        REQUIRE(first.gotAll());
        REQUIRE(first.getPath() == "/one");
        REQUIRE(buffer.readableBytes() ==
                std::string("GET /two HTTP/1.1\r\nHost: local\r\n\r\n").size());

        first.reset();
        REQUIRE(first.parseRequest(&buffer));
        REQUIRE(first.gotAll());
        REQUIRE(first.getPath() == "/two");
        REQUIRE(first.getHeader("host") == "local");
    }

    {
        Buffer buffer;
        HttpParser parser;
        buffer.append("GET /bad HTTP/1.1 extra\r\n\r\n");
        REQUIRE(!parser.parseRequest(&buffer));
        REQUIRE(!parser.gotAll());
    }

    {
        Buffer buffer;
        HttpParser parser;
        buffer.append("GET /bad HTTP/1.1\r\nMissingColon\r\n\r\n");
        REQUIRE(!parser.parseRequest(&buffer));
        REQUIRE(!parser.gotAll());
    }

    for (const char* contentLength : {"", "-1", "+1", "1x", "999999999999999999999999999999"}) {
        Buffer buffer;
        HttpParser parser;
        buffer.append("POST /bad HTTP/1.1\r\nContent-Length: ");
        buffer.append(contentLength, std::char_traits<char>::length(contentLength));
        buffer.append("\r\n\r\n");
        REQUIRE(!parser.parseRequest(&buffer));
        REQUIRE(!parser.gotAll());
    }

    for (const char* headers : {
             "Transfer-Encoding: chunked\r\n",
             "Content-Length: 1\r\nContent-Length: 1\r\n",
         }) {
        Buffer buffer;
        HttpParser parser;
        buffer.append("POST /bad HTTP/1.1\r\n");
        buffer.append(headers, std::char_traits<char>::length(headers));
        buffer.append("\r\n");
        REQUIRE(!parser.parseRequest(&buffer));
        REQUIRE(!parser.gotAll());
    }
}
