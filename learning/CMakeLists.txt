# learning/CMakeLists.txt
# 学习轨道 CMake 入口（教学代码：learning/ds-c/orig/ + 教学测试）
# 由根 CMakeLists.txt 在 LEARNING_BUILD=ON 时 add_subdirectory 引入
# 严禁引用 engineering/ 任何路径

cmake_minimum_required(VERSION 3.20)

project(LearningTrack
    VERSION 1.0.0
    DESCRIPTION "Learning Track: Hand-crafted data structures & algorithms"
    LANGUAGES C CXX
)

set(_dual_track_guard "${CMAKE_CURRENT_LIST_DIR}/../cmake/dual-track-guard.cmake")
if(EXISTS "${_dual_track_guard}")
    include("${_dual_track_guard}")
    guard_no_cross_track_reference("engineering/")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build learning tests" ON)

# S6 起学习层启用测试发现（enable_testing 必须显式调用 ctest 才能发现）
if(BUILD_TESTING)
    enable_testing()
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wno-sign-compare)
endif()

find_package(Threads REQUIRED)

# 学习轨道不依赖 googletest；如有需要可在此处启用
# S6 起：学习层启用 GTest，关联到工程层 vendored 的 googletest 子模块
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../third_part/googletest")
    set(_gtest_build_dir "${CMAKE_BINARY_DIR}/_googletest_build")
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../third_part/googletest"
        "${_gtest_build_dir}"
    )
endif()

# 接口库：暴露 learning/include 给学习轨道自身使用
add_library(learning_includes INTERFACE)
target_include_directories(learning_includes
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

# 教学数据结构（从 src/ds 整体迁入）
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/ds-c/orig/CMakeLists.txt")
    add_subdirectory(ds-c/orig)
endif()

# 教学算法（由 S4 algo-split 从 engineering/src/algo/ 迁出）
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/algo-c/CMakeLists.txt")
    add_subdirectory(algo-c)
endif()

# 代码答案集合（LeetCode + 面试题）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/code-solutions/CMakeLists.txt")
    add_subdirectory(code-solutions)
endif()

# 学习层测试（S6 引入：leetcode 题解测试）
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/code-solutions/c/test/CMakeLists.txt")
    add_subdirectory(code-solutions/c/test)
endif()

message(STATUS "Learning track: ${PROJECT_NAME} v${PROJECT_VERSION}")