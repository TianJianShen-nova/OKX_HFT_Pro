cmake_minimum_required(VERSION 3.10)
set(CMAKE_BUILD_TYPE Release)
project(HFT)

set(CMAKE_CXX_STANDARD 11)

# 查找 OpenSSL
find_package(OpenSSL REQUIRED)

# 添加 WebSocket++ 头文件路径
include_directories(/usr/local/include)

# 添加可执行文件
add_executable(HFT main.cpp)

# 链接必要的库
target_link_libraries(HFT pthread OpenSSL::SSL OpenSSL::Crypto)