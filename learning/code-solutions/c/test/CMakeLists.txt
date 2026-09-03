# learning/code-solutions/c/test/CMakeLists.txt
# 学习层测试调度器：仅 c/ 题解有测试，interview/ 不在此处
cmake_minimum_required(VERSION 3.20)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/c/CMakeLists.txt")
    add_subdirectory(c)
endif()
