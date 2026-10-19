cmake_minimum_required(VERSION 3.10)

project(AegisNet VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
include(CTest)

file(GLOB_RECURSE CORE_SOURCES
        "src/base/*.cpp"
        "src/net/*.cpp"
        "src/pool/*.cpp"
        "src/tcp/*.cpp"
)

add_library(aegisnet_core STATIC ${CORE_SOURCES})
target_include_directories(aegisnet_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(aegisnet_core PRIVATE -Wall -Wextra)
target_link_libraries(aegisnet_core PUBLIC Threads::Threads)

add_executable(AegisNetServer main.cpp)
target_link_libraries(AegisNetServer PRIVATE aegisnet_core)

option(AEGISNET_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

function(aegisnet_enable_sanitizers target)
    if(AEGISNET_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_libraries(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

aegisnet_enable_sanitizers(aegisnet_core)
aegisnet_enable_sanitizers(AegisNetServer)

if(BUILD_TESTING)
    # Keep fast unit tests available in both normal and sanitizer builds.
    add_executable(buffer_tests tests/buffer_tests.cpp)
    target_link_libraries(buffer_tests PRIVATE aegisnet_core)
    target_compile_options(buffer_tests PRIVATE -Wall -Wextra)
    aegisnet_enable_sanitizers(buffer_tests)
    add_test(NAME buffer_tests COMMAND buffer_tests)

    add_executable(http_parser_tests tests/http_parser_tests.cpp)
    target_link_libraries(http_parser_tests PRIVATE aegisnet_core)
    target_compile_options(http_parser_tests PRIVATE -Wall -Wextra)
    aegisnet_enable_sanitizers(http_parser_tests)
    add_test(NAME http_parser_tests COMMAND http_parser_tests)

    add_executable(channel_tests tests/channel_tests.cpp)
    target_link_libraries(channel_tests PRIVATE aegisnet_core)
    target_compile_options(channel_tests PRIVATE -Wall -Wextra)
    aegisnet_enable_sanitizers(channel_tests)
    add_test(NAME channel_tests COMMAND channel_tests)

    add_executable(tcp_connection_tests tests/tcp_connection_tests.cpp)
    target_link_libraries(tcp_connection_tests PRIVATE aegisnet_core)
    target_compile_options(tcp_connection_tests PRIVATE -Wall -Wextra)
    aegisnet_enable_sanitizers(tcp_connection_tests)
    add_test(NAME tcp_connection_tests COMMAND tcp_connection_tests)

    add_executable(backpressure_tests tests/backpressure_tests.cpp)
    target_link_libraries(backpressure_tests PRIVATE aegisnet_core)
    target_compile_options(backpressure_tests PRIVATE -Wall -Wextra)
    aegisnet_enable_sanitizers(backpressure_tests)
    add_test(NAME backpressure_tests COMMAND backpressure_tests)

    add_executable(timer_tests tests/timer_tests.cpp)
    target_link_libraries(timer_tests PRIVATE aegisnet_core)
    target_compile_options(timer_tests PRIVATE -Wall -Wextra)
    aegisnet_enable_sanitizers(timer_tests)
    add_test(NAME timer_tests COMMAND timer_tests)

    add_executable(logging_tests tests/logging_tests.cpp)
    target_link_libraries(logging_tests PRIVATE aegisnet_core)
    target_compile_options(logging_tests PRIVATE -Wall -Wextra)
    aegisnet_enable_sanitizers(logging_tests)
    add_test(NAME logging_tests COMMAND logging_tests)
endif()
