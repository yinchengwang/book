# learning/code-solutions/c/test/c/CMakeLists.txt
# LeetCode 题解的 ctest 测试（S6 起迁移到学习层）

cmake_minimum_required(VERSION 3.20)

# gtest_main 来自 vendored googletest（learning/CMakeLists.txt 已 add_subdirectory）
include(GoogleTest)

# 收集本目录所有 .cpp 测试源
file(GLOB LEETCODE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
message(STATUS "LEETCODE_TEST_SOURCES count: ${LEETCODE_TEST_SOURCES}")

# 创建测试可执行文件
add_executable(leetcode_tests ${LEETCODE_TEST_SOURCES})

target_include_directories(leetcode_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

target_link_libraries(leetcode_tests
    PRIVATE
        GTest::gtest_main
        leetcode-solutions
        Threads::Threads
)

set_target_properties(leetcode_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# gtest_discover_tests
gtest_discover_tests(leetcode_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "leetcode_tests"
)
