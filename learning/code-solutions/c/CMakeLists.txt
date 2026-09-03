# LeetCode C/C++ 题解（学习层）
# 路径：learning/code-solutions/c/ 包含 src/c/ 与 src/cpp/ 两套题解

file(GLOB LEETCODE_C_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/c/*.c")
file(GLOB LEETCODE_CPP_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "src/cpp/*.cpp")

add_library(leetcode-solutions STATIC
    ${LEETCODE_C_SOURCES}
    ${LEETCODE_CPP_SOURCES}
)

target_include_directories(leetcode-solutions PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/leetcode
)

target_link_libraries(leetcode-solutions PUBLIC learning_includes)

set_target_properties(leetcode-solutions PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    FOLDER "Libraries"
)

message(STATUS "LEETCODE_C_SOURCES count: ${LEETCODE_C_SOURCES}")
message(STATUS "LEETCODE_CPP_SOURCES count: ${LEETCODE_CPP_SOURCES}")