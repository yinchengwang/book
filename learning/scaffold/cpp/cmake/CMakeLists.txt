cmake_minimum_required(VERSION 3.10)
project(cpp_cmake_demo LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 注入宏定义
add_definitions(
    -DAPP_VERSION="\"${PROJECT_VERSION}\""
    -DBUILD_TYPE="\"${CMAKE_BUILD_TYPE}\""
)

# 可执行文件
add_executable(${PROJECT_NAME} main.cpp)

# 头文件目录（演示）
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# C++ 标准库链接（通常不需要显式）
target_link_libraries(${PROJECT_NAME} PRIVATE stdc++)

# 警告选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# 打印构建信息
message(STATUS "============================================")
message(STATUS "Project:   ${PROJECT_NAME}")
message(STATUS "Version:   ${PROJECT_VERSION}")
message(STATUS "Build:     ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX:       ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ std:   ${CMAKE_CXX_STANDARD}")
message(STATUS "============================================")