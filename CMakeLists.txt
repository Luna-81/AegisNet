cat << 'EOF' > CMakeLists.txt
# 1. 声明要求的最低 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 2. 声明项目名称、版本号及使用的语言
project(AegisNet VERSION 1.0.0 LANGUAGES CXX)

# 3. 🎯 核心设置：强制使用 C++17 标准 (你的结构化绑定全靠它)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 4. 开启最高级别的编译器警告 (向面试官展示你对代码质量的极高要求)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O2 -pthread")

# 5. 设置头文件搜索路径 (这样你的 #include "base/Logger.h" 才能找到文件)
include_directories(${CMAKE_SOURCE_DIR})

# 6. 收集所有核心模块的源文件 (.cpp)
file(GLOB_RECURSE CORE_SOURCES
"base/*.cpp"
"net/*.cpp"
"pool/*.cpp"
"tcp/*.cpp"
)

# 7. 把核心代码编译成一个静态库 (libaegisnet_core.a)
add_library(aegisnet_core STATIC ${CORE_SOURCES})

# 8. 编译你的终极示例程序 (假设你的应用层主文件叫 WebChatServer.cpp)
# ⚠️ 注意：如果你的文件名不叫这个，请把 WebChatServer.cpp 改成你实际的名字！
add_executable(WebChatServer example/WebChatServer.cpp)

# 9. 把核心库和多线程库 (pthread) 链接到你的程序上
target_link_libraries(WebChatServer aegisnet_core pthread)
EOF