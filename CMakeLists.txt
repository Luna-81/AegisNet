# 1. 声明要求的最低 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 2. 声明项目名称、版本号及使用的语言
project(AegisNet VERSION 1.0.0 LANGUAGES CXX)

# 3. 🎯 核心设置：强制使用 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 4. 开启最高级别的编译器警告
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O2 -pthread")

# 5. 告诉编译器去哪里找头文件
include_directories(${CMAKE_SOURCE_DIR})

# 6. 🎯 收集 src 目录下的所有源文件
file(GLOB_RECURSE CORE_SOURCES
        "src/base/*.cpp"
        "src/net/*.cpp"
        "src/pool/*.cpp"
        "src/tcp/*.cpp"
)

# 7. 把核心代码编译成一个静态库
add_library(aegisnet_core STATIC ${CORE_SOURCES})

# 8. 🎯 编译根目录下的 main.cpp
add_executable(AegisNetServer main.cpp)

# 9. 链接核心库和多线程库
target_link_libraries(AegisNetServer aegisnet_core pthread)