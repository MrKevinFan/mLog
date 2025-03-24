cmake_minimum_required(VERSION 3.19)
project(mlog VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)

set(MLOG_SOURCES
    src/main.c
        src/mlog.c
)

add_executable(mlog)
target_sources(mlog PRIVATE ${MLOG_SOURCES})
target_include_directories(mlog PRIVATE include src)