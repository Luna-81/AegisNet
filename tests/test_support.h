#pragma once

#include <cstdlib>
#include <iostream>

inline void require(bool condition, const char* expression, int line) {
    if (!condition) {
        std::cerr << "Test failure at line " << line << ": " << expression << '\n';
        std::exit(EXIT_FAILURE);
    }
}

#define REQUIRE(expression) require((expression), #expression, __LINE__)
