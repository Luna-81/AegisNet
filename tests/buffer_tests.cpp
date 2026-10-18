#include "src/tcp/Buffer.h"

#include "test_support.h"

#include <string>

int main() {
    Buffer buffer(8);

    buffer.append("hello", 5);
    REQUIRE(buffer.readableBytes() == 5);
    REQUIRE(std::string(buffer.peek(), buffer.readableBytes()) == "hello");

    REQUIRE(buffer.retrieveAsString(2) == "he");
    REQUIRE(buffer.readableBytes() == 3);
    REQUIRE(buffer.retrieveAllAsString() == "llo");
    REQUIRE(buffer.readableBytes() == 0);

    buffer.append("a line\r\nnext", 12);
    const char* crlf = buffer.findCRLF();
    REQUIRE(crlf != nullptr);
    REQUIRE(std::string(buffer.peek(), crlf) == "a line");
    buffer.retrieveUntil(crlf + 2);
    REQUIRE(buffer.retrieveAllAsString() == "next");

    buffer.append("payload", 7);
    buffer.prependInt32(42);
    REQUIRE(buffer.peekInt32() == 42);
    REQUIRE(buffer.readInt32() == 42);
    REQUIRE(buffer.retrieveAllAsString() == "payload");

    // This append is larger than the initial writable region and exercises growth.
    buffer.append("abcdefghijk", 11);
    REQUIRE(buffer.retrieveAllAsString() == "abcdefghijk");
}
