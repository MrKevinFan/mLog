cmake_minimum_required(VERSION 3.19)
project(mLog VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")

set(MLOG_SOURCES
    src/mLog.c
)
# 主库
add_library(mLog SHARED)
# 主程序
#add_executable(mLog)
target_sources(mLog PRIVATE ${MLOG_SOURCES})
target_include_directories(mLog PRIVATE include src)

# 示例程序
add_executable(example example/main.c)
target_include_directories(example PRIVATE include src)
target_link_libraries(example mLog)