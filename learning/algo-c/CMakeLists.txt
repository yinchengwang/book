# 学习性算法库（手写经典算法、LeetCode 题解、周赛题解）
# 由 S4 algo-split 从 engineering/src/algo/ 迁出
# 不引用 engineering/algo-prod/，严格双轨隔离

file(GLOB_RECURSE ALGO_C_SOURCES CONFIGURE_DEPENDS "*.c")

add_library(algo-c STATIC ${ALGO_C_SOURCES})

target_include_directories(algo-c PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(algo-c PUBLIC learning_includes)

target_compile_options(algo-c PRIVATE
    -Wall -Wextra -Wpedantic -Wno-sign-compare
)

set_target_properties(algo-c PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    FOLDER "Libraries"
)

message(STATUS "ALGO_C_SOURCES count: ${ALGO_C_SOURCES}")